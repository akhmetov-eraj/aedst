#pragma once

#include <string_view>

/// Connect procedure paths, `/<proto package>.<Service>/<Method>`, from the
/// single proto source of truth at apps/server/api/proto — the same contract
/// apps/web (Connect-ES), apps/apple (connect-swift) and apps/android use.
namespace aedst::network::endpoints {

// aedst.auth.v1.AuthService
inline constexpr std::string_view kAuthLogin = "/aedst.auth.v1.AuthService/Login";
inline constexpr std::string_view kAuthRefreshToken = "/aedst.auth.v1.AuthService/RefreshToken";
inline constexpr std::string_view kAuthLogout = "/aedst.auth.v1.AuthService/Logout";

// aedst.users.v1.UsersService
inline constexpr std::string_view kUsersRegisterUser = "/aedst.users.v1.UsersService/RegisterUser";
inline constexpr std::string_view kUsersGetUser = "/aedst.users.v1.UsersService/GetUser";

// aedst.profile.v1.ProfileService
inline constexpr std::string_view kProfileGetProfile = "/aedst.profile.v1.ProfileService/GetProfile";
inline constexpr std::string_view kProfileUpdateProfile = "/aedst.profile.v1.ProfileService/UpdateProfile";

} // namespace aedst::network::endpoints
