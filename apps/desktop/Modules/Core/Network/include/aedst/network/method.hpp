#pragma once

#include <string_view>

namespace aedst::network {

/// Whether replaying the call after a transport failure is safe. Only
/// `Idempotent` calls are retried automatically (see RetryPolicy); when in doubt
/// declare `Unknown` - a duplicated Login/CreateTask is worse than a surfaced error.
///
/// The server's protos carry no `idempotency_level` option yet, so this is
/// declared by hand in aedst/contract/methods.hpp. Once the protos say
/// `option idempotency_level = NO_SIDE_EFFECTS;` on read RPCs, that becomes the
/// single source of truth (and unlocks Connect GET requests).
enum class Idempotency {
    Unknown,
    Idempotent,
};

/// Whether the AuthInterceptor attaches the bearer token. Mirrors the server's
/// `publicProcedures` set (apps/server/cmd/server/main.go).
enum class AuthRequirement {
    Required,
    Public,
};

/// Type-erased description of one RPC: `/<proto package>.<Service>/<Method>`.
struct MethodInfo {
    std::string_view path;
    Idempotency idempotency{Idempotency::Unknown};
    AuthRequirement auth{AuthRequirement::Required};
};

/// A typed RPC: binds the procedure path to its request/response messages so
/// ConnectClient::unary<Req, Res> cannot be called with mismatched types.
template <typename Req, typename Res>
struct Method {
    using Request = Req;
    using Response = Res;

    MethodInfo info;
};

template <typename Req, typename Res>
[[nodiscard]] constexpr Method<Req, Res> makeMethod(std::string_view path, AuthRequirement auth,
                                                    Idempotency idempotency = Idempotency::Unknown) noexcept {
    return Method<Req, Res>{MethodInfo{.path = path, .idempotency = idempotency, .auth = auth}};
}

} // namespace aedst::network
