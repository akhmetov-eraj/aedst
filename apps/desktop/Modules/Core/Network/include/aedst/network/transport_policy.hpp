#pragma once

#include "aedst/network/api_error.hpp"

#include <QString>
#include <QUrl>

namespace aedst::network {

/// Transport-security policy for the API base URL, as pure functions (no I/O, no
/// event loop) so it is unit-testable and can run at startup before any
/// ConnectClient exists.
///
/// The rule: the API is https. Plain http is a development convenience for a
/// server on the same machine, so it is allowed only when ALL hold:
///  - the host is loopback: `localhost`, `*.localhost`, 127.0.0.0/8, `::1`;
///  - the caller passes allowInsecureLoopback (see insecureLoopbackAllowedByDefault:
///    Debug builds, or the AEDST_ALLOW_INSECURE_LOCALHOST=1 runtime opt-in).
/// Anything else that is not https is rejected, as are URLs that could smuggle
/// credentials or ambiguity into every request: empty host, `user:pass@`
/// userinfo, a fragment, or a query string (the base is a prefix for RPC paths).

/// True for `localhost`, `*.localhost` (RFC 6761), any 127.0.0.0/8 address and
/// `::1`. A trailing dot and IPv6 brackets are tolerated. Names are NOT resolved:
/// an attacker-controlled DNS name that points at 127.0.0.1 is not loopback here.
[[nodiscard]] bool isLoopbackHost(const QString& host);

/// Debug builds (no NDEBUG): true. Otherwise true only when the environment
/// variable AEDST_ALLOW_INSECURE_LOCALHOST is one of 1, true, yes, on
/// (case-insensitive). Read on every call; ClientConfig uses it as its default.
[[nodiscard]] bool insecureLoopbackAllowedByDefault();

/// Ok when `baseUrl` satisfies the policy above, otherwise an
/// ApiErrorKind::InvalidArgument error whose message says what to fix (it never
/// echoes credentials from the URL). Not retryable.
[[nodiscard]] ApiResult<void> validateEndpoint(const QUrl& baseUrl, bool allowInsecureLoopback);

} // namespace aedst::network
