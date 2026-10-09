import AedstCore
import Foundation
@testable import Auth

extension Session {
    static let stub = Session(
        userID: "user-1",
        accessTokenExpiresAt: .distantFuture,
        refreshTokenExpiresAt: .distantFuture
    )
}

extension TokenPairDTO {
    static let stub = TokenPairDTO(
        accessToken: "access-1",
        refreshToken: "refresh-1",
        accessTokenExpiresInSeconds: 900,
        refreshTokenExpiresInSeconds: 86_400
    )
}

/// Scripted `AuthRemoteDataSource`; results are consumed in order, the last repeats.
actor FakeAuthRemote: AuthRemoteDataSource {
    var loginResults: [Outcome<LoginResultDTO>]
    var refreshResults: [Outcome<TokenPairDTO>]
    var logoutResult: Outcome<Void>
    private(set) var loginCalls: [(email: String, deviceID: String)] = []
    private(set) var refreshCalls: [String] = []
    private(set) var logoutCalls: [String] = []

    init(
        loginResults: [Outcome<LoginResultDTO>] = [.success(LoginResultDTO(userID: "user-1", tokens: .stub))],
        refreshResults: [Outcome<TokenPairDTO>] = [.success(.stub)],
        logoutResult: Outcome<Void> = .success(())
    ) {
        self.loginResults = loginResults
        self.refreshResults = refreshResults
        self.logoutResult = logoutResult
    }

    func login(email: String, password: String, deviceID: String) async -> Outcome<LoginResultDTO> {
        loginCalls.append((email, deviceID))
        return loginResults.count > 1 ? loginResults.removeFirst() : loginResults[0]
    }

    func refresh(refreshToken: String) async -> Outcome<TokenPairDTO> {
        refreshCalls.append(refreshToken)
        return refreshResults.count > 1 ? refreshResults.removeFirst() : refreshResults[0]
    }

    func logout(refreshToken: String) async -> Outcome<Void> {
        logoutCalls.append(refreshToken)
        return logoutResult
    }
}

/// `AuthRepository` double recording how it was used.
actor SpyAuthRepository: AuthRepository {
    private let loginResult: Outcome<Session>
    private let signInResult: Outcome<Session>
    private let mode: SignInMode
    private(set) var loginCallCount = 0
    private(set) var signInCallCount = 0
    private(set) var lastSignInDeviceID: String?
    private(set) var lastCredentials: Credentials?
    private(set) var lastDeviceID: String?
    var storedSession: Session?

    init(
        loginResult: Outcome<Session>,
        storedSession: Session? = nil,
        mode: SignInMode = .password,
        signInResult: Outcome<Session> = .failure(.unsupported)
    ) {
        self.loginResult = loginResult
        self.mode = mode
        self.signInResult = signInResult
        self.storedSession = storedSession
    }

    nonisolated var signInMode: SignInMode { mode }

    func signIn(deviceID: String) async -> Outcome<Session> {
        signInCallCount += 1
        lastSignInDeviceID = deviceID
        return signInResult
    }

    func login(with credentials: Credentials, deviceID: String) async -> Outcome<Session> {
        loginCallCount += 1
        lastCredentials = credentials
        lastDeviceID = deviceID
        return loginResult
    }

    func logout() async -> Outcome<Void> { .success(()) }

    func currentSession() async -> Session? { storedSession }
}
