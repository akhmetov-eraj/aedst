#pragma once

#include "Domain/search_hit.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace aedst::feature::search {

/// U+E000 / U+E001 (Private Use Area) in UTF-8: the server wraps every matched
/// term in these so clients highlight natively without parsing HTML
/// (aedst.search.v1.SearchHit.snippet).
inline constexpr std::string_view kHighlightStart = "\xEE\x80\x80";
inline constexpr std::string_view kHighlightEnd = "\xEE\x80\x81";

/// Splits a marked snippet into plain/highlighted runs. Tolerant by design
/// (server text is untrusted): a missing end marker highlights to the end, a
/// stray end marker is dropped, a repeated start marker is ignored, and empty
/// runs are omitted. Adjacent runs never share a `highlighted` value.
[[nodiscard]] std::vector<SnippetSegment> parseSnippet(std::string_view marked);

/// Minimum/maximum query lengths of aedst.search.v1.SearchRequest.query, in code points.
inline constexpr std::size_t kMinQueryChars = 1;
inline constexpr std::size_t kMaxQueryChars = 200;

/// Trims whitespace and cuts to kMaxQueryChars code points (never mid-character).
/// Empty result = nothing to search for.
[[nodiscard]] std::string normalizeQuery(std::string_view raw);

/// Unicode code points in valid UTF-8 (lead bytes only).
[[nodiscard]] std::size_t countCodePoints(std::string_view utf8);

/// SuggestRequest.prefix limit, in code points.
inline constexpr std::size_t kMaxSuggestPrefixChars = 100;

} // namespace aedst::feature::search
