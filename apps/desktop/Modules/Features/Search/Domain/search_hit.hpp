#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::feature::search {

enum class DocumentKind {
    Unknown,
    Task,
    Media,
};

[[nodiscard]] constexpr std::string_view toString(DocumentKind kind) noexcept {
    switch (kind) {
    case DocumentKind::Task:
        return "task";
    case DocumentKind::Media:
        return "media";
    case DocumentKind::Unknown:
        break;
    }
    return "unknown";
}

/// One run of snippet text; `highlighted` runs were wrapped in U+E000 ... U+E001.
struct SnippetSegment {
    std::string text;
    bool highlighted{false};

    friend bool operator==(const SnippetSegment&, const SnippetSegment&) = default;
};

struct SearchHit {
    DocumentKind kind{DocumentKind::Unknown};
    std::string id;
    std::string title;
    std::vector<SnippetSegment> snippet;
    /// In-app relative route ("/tasks/<id>"); opened through Router::openLink.
    std::string link;
    float score{0.0F};
    std::optional<std::chrono::system_clock::time_point> updatedAt;

    friend bool operator==(const SearchHit&, const SearchHit&) = default;
};

struct SearchQuery {
    std::string text;
    /// Empty = every kind.
    std::vector<DocumentKind> kinds;
    std::string pageToken;
    int pageSize{20};
};

struct SearchPage {
    std::vector<SearchHit> hits;
    std::string nextPageToken;
    /// Approximate; first page only.
    int totalEstimate{0};
};

} // namespace aedst::feature::search
