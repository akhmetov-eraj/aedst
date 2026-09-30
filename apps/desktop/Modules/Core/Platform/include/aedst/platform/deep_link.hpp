#pragma once

#include "aedst/core/outcome.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aedst::platform {

inline constexpr std::size_t kMaxDeepLinkLength = 2048;
inline constexpr std::size_t kMaxDeepLinkParams = 16;
inline constexpr std::size_t kMaxDeepLinkKeyLength = 32;
inline constexpr std::size_t kMaxDeepLinkValueLength = 512;
inline constexpr std::size_t kMaxDeepLinkSegmentLength = 64;

/// A validated `aedst://` URL. `route` is `<host>[/<segment>]` (host already
/// lower-cased) and is guaranteed to match the allow-list; `params` values are
/// percent-decoded, valid UTF-8 and free of control characters.
struct DeepLink {
    std::string route;
    std::vector<std::pair<std::string, std::string>> params;

    friend bool operator==(const DeepLink&, const DeepLink&) = default;
};

/// True when the command-line argument is *shaped* like a deep link
/// (case-insensitive `aedst:` prefix). It says nothing about validity.
[[nodiscard]] bool looksLikeDeepLink(std::string_view argument) noexcept;

/// Strict parser for `aedst://<host>[/<segment>][?k=v&k2=v2]`.
///
/// Deep links arrive from outside the app (browser, OS, a second launch), so
/// they are hostile input. The parser is Qt-free and rejects, rather than
/// repairs, anything unusual:
///  - empty input, length > kMaxDeepLinkLength, other schemes;
///  - control characters, non-ASCII bytes, spaces, backslashes, fragments (`#`);
///  - userinfo (`@`) / ports (`:`): the host must be in the allow-list, which is
///    auth/callback, tasks[/<id>] and settings (see deep_link.cpp);
///  - `.`/`..`/empty/non-identifier path segments;
///  - `+` in the query, malformed percent escapes, decoded control characters,
///    invalid UTF-8, duplicate/oversize/too many parameters.
/// One trailing `/` after the path is tolerated (some OSes append it).
[[nodiscard]] core::Outcome<DeepLink> parseDeepLink(std::string_view url);

} // namespace aedst::platform
