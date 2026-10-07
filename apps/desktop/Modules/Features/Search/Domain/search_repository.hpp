#pragma once

#include "Domain/search_hit.hpp"
#include "aedst/core/outcome.hpp"

#include <string>
#include <vector>

namespace aedst::feature::search {

/// Domain-owned port over aedst.search.v1.SearchService (results are always the
/// caller's own documents - the server filters by the authenticated user).
class SearchRepository {
public:
    SearchRepository() = default;
    SearchRepository(const SearchRepository&) = delete;
    SearchRepository& operator=(const SearchRepository&) = delete;
    SearchRepository(SearchRepository&&) = delete;
    SearchRepository& operator=(SearchRepository&&) = delete;
    virtual ~SearchRepository() = default;

    virtual void search(const SearchQuery& query, core::Completion<SearchPage> done) = 0;
    virtual void suggest(const std::string& prefix, int limit, core::Completion<std::vector<std::string>> done) = 0;
};

} // namespace aedst::feature::search
