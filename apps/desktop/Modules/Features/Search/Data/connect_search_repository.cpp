#include "Data/connect_search_repository.hpp"

#include "Domain/snippet.hpp"
#include "aedst/contract/methods.hpp"
#include "aedst/contract/timestamps.hpp"
#include "aedst/network/connect_client.hpp"

#include <algorithm>
#include <utility>

namespace aedst::feature::search {
namespace {

[[nodiscard]] DocumentKind kindFromProto(searchv1::DocumentKind kind) {
    switch (kind) {
    case searchv1::DOCUMENT_KIND_TASK:
        return DocumentKind::Task;
    case searchv1::DOCUMENT_KIND_MEDIA:
        return DocumentKind::Media;
    default:
        break;
    }
    return DocumentKind::Unknown;
}

[[nodiscard]] searchv1::DocumentKind kindToProto(DocumentKind kind) {
    switch (kind) {
    case DocumentKind::Task:
        return searchv1::DOCUMENT_KIND_TASK;
    case DocumentKind::Media:
        return searchv1::DOCUMENT_KIND_MEDIA;
    case DocumentKind::Unknown:
        break;
    }
    return searchv1::DOCUMENT_KIND_UNSPECIFIED;
}

} // namespace

ConnectSearchRepository::ConnectSearchRepository(std::shared_ptr<network::ConnectClient> client)
    : client_(std::move(client)) {}

SearchHit ConnectSearchRepository::toDomain(const searchv1::SearchHit& hit) {
    return SearchHit{
        .kind = kindFromProto(hit.kind()),
        .id = hit.id(),
        .title = hit.title(),
        .snippet = parseSnippet(hit.snippet()),
        .link = hit.link(),
        .score = hit.score(),
        .updatedAt = contract::toOptionalTimePoint(hit.has_updated_at(), hit.updated_at()),
    };
}

void ConnectSearchRepository::search(const SearchQuery& query, core::Completion<SearchPage> done) {
    searchv1::SearchRequest request;
    request.set_query(query.text);
    for (const DocumentKind kind : query.kinds) {
        if (const auto proto = kindToProto(kind); proto != searchv1::DOCUMENT_KIND_UNSPECIFIED) {
            request.add_kinds(proto); // not_in: [0] on the server
        }
    }
    request.set_page_size(std::clamp(query.pageSize, 0, 50));
    request.set_page_token(query.pageToken);
    client_->unary(contract::kSearchSearch, request,
                   [done = std::move(done)](network::ApiResult<searchv1::SearchResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       SearchPage page;
                       page.hits.reserve(static_cast<std::size_t>(result->hits_size()));
                       for (const auto& hit : result->hits()) {
                           page.hits.push_back(toDomain(hit));
                       }
                       page.nextPageToken = result->next_page_token();
                       page.totalEstimate = result->total_estimate();
                       done(std::move(page));
                   });
}

void ConnectSearchRepository::suggest(const std::string& prefix, int limit,
                                      core::Completion<std::vector<std::string>> done) {
    searchv1::SuggestRequest request;
    request.set_prefix(prefix); // callers pass a normalizeQuery() result of <= 100 code points
    request.set_limit(std::clamp(limit, 0, 20));
    client_->unary(contract::kSearchSuggest, request,
                   [done = std::move(done)](network::ApiResult<searchv1::SuggestResponse> result) {
                       if (!result) {
                           done(core::fail(result.error().toCoreError()));
                           return;
                       }
                       done(std::vector<std::string>(result->suggestions().begin(), result->suggestions().end()));
                   });
}

} // namespace aedst::feature::search
