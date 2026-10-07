#pragma once

#include "aedst/platform/notification_service.hpp"

#include <cstddef>
#include <string>

namespace aedst::platform {

/// Longest link accepted into a toast's activation arguments.
inline constexpr std::size_t kMaxToastLinkChars = 512;

/// True for an in-app route that may travel through a toast's activation arguments: it starts with one `/`
/// (not `//`), is not longer than kMaxToastLinkChars and contains no control characters or quotes. Anything
/// else is dropped, so a hostile notification cannot smuggle a URL or markup into the activation payload. The
/// receiver (Router::openLink) validates the route again.
[[nodiscard]] bool isActivatableLink(const std::string& link);

/// Windows toast XML (ToastGeneric template) for a request: title and body as text nodes, escaped and stripped of
/// characters XML 1.0 forbids; `launch` and the action button only when the link passes isActivatableLink().
/// Pure, platform independent and unit-tested; only the Windows service feeds it to the toast API.
[[nodiscard]] std::wstring buildToastXml(const NotificationRequest& request);

} // namespace aedst::platform
