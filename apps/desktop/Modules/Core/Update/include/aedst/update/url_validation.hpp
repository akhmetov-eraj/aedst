#pragma once

#include <string_view>

namespace aedst::update {

/// Strict https URL check used for every URL that comes out of a manifest (and
/// for the manifest URL itself). Accepts only:
///  - scheme `https` (case-insensitive), at most 2048 bytes;
///  - no userinfo (`user:pass@`), no IPv6 literals, no backslashes, spaces,
///    control characters or non-ASCII bytes;
///  - a DNS-style host (labels of [A-Za-z0-9-], not starting/ending with `-`)
///    and an optional numeric port in 1..65535.
/// Path, query and fragment are not interpreted beyond the character check.
[[nodiscard]] bool isSecureHttpsUrl(std::string_view url);

} // namespace aedst::update
