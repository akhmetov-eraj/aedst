import AedstCore
import AedstNetwork
import Connect

/// `AuthRemoteDataSource` backed by the generated `AuthService` Connect client.
///
/// Requires the generated stubs (`make generate`, see `apps/apple/buf.gen.yaml`):
/// they are produced from `apps/server/api/proto` and are deliberately not
/// committed. Login, RefreshToken and Logout are all called through the *plain*
/// (interceptor-free) client — they authenticate with credentials / the refresh
/// token in the body, never a bearer access token.
///
/// The generated type names (`Aedst_Auth_V1_*`) follow connect-swift 1.2.3 /
/// swift-protobuf naming; the first real build (`make bootstrap`) is their
/// acceptance test.
public struct ConnectAuthRemoteDataSource: AuthRemoteDataSource {
    private let client: Aedst_Auth_V1_AuthServiceClient

    public init(client: ProtocolClient) {
        self.client = Aedst_Auth_V1_AuthServiceClient(client: client)
    }

    public func login(email: String, password: String, deviceID: String) async -> Outcome<LoginResultDTO> {
        var request = Aedst_Auth_V1_LoginRequest()
        request.email = email
        request.password = password
        request.deviceID = deviceID

        let response = await client.login(request: request, headers: [:])
        switch response.result {
        case .success(let message):
            guard message.hasTokens, !message.tokens.accessToken.isEmpty, !message.tokens.refreshToken.isEmpty else {
                return .failure(.decoding(reason: "LoginResponse carried no tokens"))
            }
            return .success(LoginResultDTO(userID: message.userID, tokens: TokenPairDTO(message.tokens)))
        case .failure(let error):
            return .failure(NetworkErrorMapping.map(error))
        }
    }

    public func refresh(refreshToken: String) async -> Outcome<TokenPairDTO> {
        var request = Aedst_Auth_V1_RefreshTokenRequest()
        request.refreshToken = refreshToken

        let response = await client.refreshToken(request: request, headers: [:])
        switch response.result {
        case .success(let message):
            guard message.hasTokens, !message.tokens.accessToken.isEmpty, !message.tokens.refreshToken.isEmpty else {
                return .failure(.decoding(reason: "RefreshTokenResponse carried no tokens"))
            }
            return .success(TokenPairDTO(message.tokens))
        case .failure(let error):
            return .failure(NetworkErrorMapping.map(error))
        }
    }

    public func logout(refreshToken: String) async -> Outcome<Void> {
        var request = Aedst_Auth_V1_LogoutRequest()
        request.refreshToken = refreshToken

        let response = await client.logout(request: request, headers: [:])
        switch response.result {
        case .success:
            return .success(())
        case .failure(let error):
            return .failure(NetworkErrorMapping.map(error))
        }
    }
}

extension TokenPairDTO {
    fileprivate init(_ proto: Aedst_Auth_V1_TokenPair) {
        self.init(
            accessToken: proto.accessToken,
            refreshToken: proto.refreshToken,
            accessTokenExpiresInSeconds: proto.accessTokenExpiresInSeconds,
            refreshTokenExpiresInSeconds: proto.refreshTokenExpiresInSeconds
        )
    }
}
