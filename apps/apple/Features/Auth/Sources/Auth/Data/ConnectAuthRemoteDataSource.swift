import AedstCore
import AedstNetwork
import Connect

// Uncomment once `make generate` (see `apps/apple/buf.gen.yaml`) has produced
// `AedstNetwork`'s `Sources/AedstNetwork/Generated/aedst/auth/v1/*.pb.swift`.
// Left commented so the rest of this package (and every other feature) keeps
// compiling before codegen has run — this is the one file per feature that
// depends on generated code, by design (see `AuthRemoteDataSource.swift`).
//
// import struct AedstNetwork.Aedst_Auth_V1_LoginRequest
// import struct AedstNetwork.Aedst_Auth_V1_LogoutRequest
// import class AedstNetwork.Aedst_Auth_V1_AuthServiceClient

/// `AuthRemoteDataSource` backed by the generated `AuthService` Connect
/// client. NOTE: not compiled in this environment — no Swift toolchain here
/// and no generated stubs yet. Un-comment the body once codegen has run;
/// the shape (`Aedst_Auth_V1_*` type names, `ResponseMessage<T>` wrapper)
/// follows connect-swift's documented codegen output but should be checked
/// against the resolved package version on first build.
public struct ConnectAuthRemoteDataSource: AuthRemoteDataSource {
    private let client: ProtocolClient

    public init(client: ProtocolClient) {
        self.client = client
    }

    public func login(email: String, password: String, deviceID: String) async -> Outcome<(userID: String, tokens: TokenPairDTO)> {
        assertionFailure(
            """
            ConnectAuthRemoteDataSource.login is a template pending codegen — \
            run `make generate` in apps/apple/, then replace this body with the \
            Aedst_Auth_V1_AuthServiceClient.login(request:) call and map its \
            LoginResponse into (userID, TokenPairDTO).
            """
        )
        // Fail gracefully instead of crashing: a debug/test build already
        // stops on the assertionFailure above; a release build without
        // generated stubs should surface a login error, not take the app down.
        return .failure(.unknown(reason: "Auth service is not available in this build."))
    }

    public func logout(refreshToken: String) async -> Outcome<Void> {
        assertionFailure(
            """
            ConnectAuthRemoteDataSource.logout is a template pending codegen — \
            see login(email:password:deviceID:) above.
            """
        )
        return .failure(.unknown(reason: "Auth service is not available in this build."))
    }
}
