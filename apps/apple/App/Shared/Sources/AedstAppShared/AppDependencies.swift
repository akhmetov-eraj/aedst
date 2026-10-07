import Admin
import AedstAnalytics
import AedstCore
import AedstMedia
import AedstNetwork
import AedstPersistence
import AedstTelemetry
import Auth
import Connect
import Foundation
import Media
import Notifications
import Search
import Tasks

/// The app's single composition root — every concrete implementation is
/// constructed exactly once, here, and only protocol types cross into
/// `Presentation`. `App/iOS`'s and `App/macOS`'s app delegates both build one
/// `AppDependencies` and hand it to the same `RootView`; nothing
/// platform-specific belongs in this type except what the delegate injects
/// (APNs registration, the icon badge, the device name).
///
/// Every external effect (Keychain, network reachability, biometrics, time,
/// telemetry) is an injectable parameter, so tests construct the whole graph
/// with doubles and no SDK ever starts in a unit test.
@MainActor
public final class AppDependencies {
    /// The `admin` role name (server default policy: `/aedst.admin.v1.AdminService/*=admin`).
    public static let adminRole = "admin"

    public let analytics: AnalyticsClient
    public let featureFlags: FeatureFlagProvider
    public let clock: any AedstClock
    public let sessionStore: any SessionStore
    public let authRepository: AuthRepository
    public let connectivity: any ConnectivityMonitoring
    public let biometrics: any BiometricAuthenticating
    public let appLockPreference: AppLockPreference
    public let consentPreference: AnalyticsConsentPreference

    /// Client that attaches the bearer token. A feature's remote data source
    /// uses it together with `authorizedRequests` (refresh-and-retry on 401).
    public let authenticatedClient: ProtocolClient
    public let authorizedRequests: AuthorizedRequestExecutor

    public let taskRepository: any TaskRepository
    public let searchRepository: any SearchRepository
    public let notificationsRepository: any NotificationsRepository
    public let mediaRepository: any MediaRepository
    public let adminRepository: any AdminRepository

    /// Background presigned-PUT uploads (one session per process).
    public let uploader: BackgroundUploader
    public let push: PushCoordinator
    public let badge: UnreadBadgeModel
    /// Links from notification taps (app delegate → `RootView`).
    public let linkInbox = AppLinkInbox()

    private let telemetry: TelemetryAnalyticsClient
    private let rolesClaim: String
    private let deviceIdentifiers: DeviceIdentifierStore
    private var servicesStarted = false
    #if canImport(MetricKit)
    private let diagnostics: DiagnosticsSubscriber
    #endif

    /// - Parameters:
    ///   - telemetry: `nil` reads `AEDST_SENTRY_DSN`/`AEDST_POSTHOG_*` from Info.plist
    ///     (empty in tests and in forks without accounts → SDKs stay off).
    ///   - registerForRemoteNotifications / setAppBadge / deviceName: platform
    ///     hooks supplied by the app delegate.
    public init(
        environment: APIEnvironment = .fromInfoPlist(),
        secureStore: any SecureStore = Keychain(),
        clock: any AedstClock = SystemClock(),
        connectivity: any ConnectivityMonitoring = NetworkPathMonitor(),
        biometrics: any BiometricAuthenticating = LocalAuthenticator(),
        telemetry: TelemetryConfiguration? = nil,
        bundle: Bundle = .main,
        deviceName: String = "Apple device",
        registerForRemoteNotifications: @escaping @MainActor () -> Void = {},
        setAppBadge: @escaping @MainActor (Int) -> Void = { _ in }
    ) {
        let telemetryClient = TelemetryAnalyticsClient(
            configuration: telemetry ?? .fromInfoPlist(
                bundle: bundle,
                apiBaseURL: environment.baseURL,
                environment: Self.name(of: environment)
            )
        )
        self.telemetry = telemetryClient
        self.analytics = telemetryClient
        self.featureFlags = telemetryClient
        self.clock = clock
        self.connectivity = connectivity
        self.biometrics = biometrics
        self.appLockPreference = AppLockPreference(store: secureStore)
        self.consentPreference = AnalyticsConsentPreference(store: secureStore)
        self.deviceIdentifiers = DeviceIdentifierStore(store: secureStore)
        self.rolesClaim = (bundle.object(forInfoDictionaryKey: "AEDST_ROLES_CLAIM") as? String)
            .flatMap { $0.isEmpty || $0.hasPrefix("$(") ? nil : $0 } ?? AccessTokenClaims.defaultRolesClaim

        // Login / RefreshToken / Logout use the interceptor-free client: they
        // cannot carry a bearer token, and this breaks the client <-> token
        // store dependency cycle.
        let plainClient = ConnectClientFactory.makePlainClient(environment: environment)
        let remote = ConnectAuthRemoteDataSource(client: plainClient)
        let refresher = AuthTokenRefresher(remote: remote, clock: clock)

        let tokenStore = KeychainTokenStore(store: secureStore, clock: clock) { refreshToken in
            await refresher.refresh(refreshToken: refreshToken)
        }
        self.sessionStore = tokenStore

        self.authRepository = ConnectAuthRepository(remote: remote, sessionStore: tokenStore, clock: clock)
        let client = ConnectClientFactory.makeClient(environment: environment, tokenProvider: tokenStore)
        let executor = AuthorizedRequestExecutor(tokenProvider: tokenStore)
        self.authenticatedClient = client
        self.authorizedRequests = executor

        self.taskRepository = ConnectTaskRepository(client: client, executor: executor)
        self.searchRepository = ConnectSearchRepository(client: client, executor: executor)
        let notifications = ConnectNotificationsRepository(client: client, executor: executor)
        self.notificationsRepository = notifications
        let media = ConnectMediaRepository(client: client, executor: executor)
        self.mediaRepository = media
        self.adminRepository = ConnectAdminRepository(client: client, executor: executor)

        self.badge = UnreadBadgeModel(repository: notifications, setAppBadge: setAppBadge)
        self.push = PushCoordinator(
            repository: notifications,
            store: secureStore,
            configuration: .fromInfoPlist(bundle: bundle, deviceName: deviceName),
            registerForRemoteNotifications: registerForRemoteNotifications
        )

        // Creating the uploader does not open the background session (lazy).
        let uploader = BackgroundUploader()
        self.uploader = uploader
        let logger = AedstLogger(category: "upload")
        // An upload that finished after the app was terminated: finish the
        // protocol (CompleteUpload) from here.
        uploader.setOrphanCompletionHandler { mediaID, status, error in
            guard error == nil, let status, (200..<300).contains(status), !mediaID.isEmpty else {
                logger.error("Background upload \(mediaID) did not complete (status \(status.map(String.init) ?? "none"))")
                return
            }
            Task { _ = await media.completeUpload(id: mediaID) }
        }

        #if canImport(MetricKit)
        self.diagnostics = DiagnosticsSubscriber(analytics: telemetryClient)
        #endif
    }

    /// Starts long-lived, launch-time services once: telemetry (consent-aware),
    /// MetricKit diagnostics, the background upload session.
    public func startServices() {
        guard !servicesStarted else { return }
        servicesStarted = true
        telemetry.start(consent: analyticsConsent)
        uploader.activate()
        #if canImport(MetricKit)
        diagnostics.start()
        #endif
    }

    // MARK: Consent

    /// Current product-analytics consent (`unknown` until the user decides).
    public var analyticsConsent: AnalyticsConsent {
        consentPreference.rawValue.flatMap(AnalyticsConsent.init(rawValue:)) ?? .unknown
    }

    /// Persists and applies the user's analytics choice.
    public func setAnalyticsConsent(_ consent: AnalyticsConsent) throws {
        try consentPreference.set(consent.rawValue)
        telemetry.setConsent(consent)
    }

    // MARK: Session-derived facts

    /// Whether the current access token carries the admin role. UX only — the
    /// server's RoleInterceptor authorizes every AdminService call.
    public func isAdmin() async -> Bool {
        guard let token = await sessionStore.currentAccessToken(),
              let claims = AccessTokenClaims(jwt: token, rolesClaim: rolesClaim)
        else { return false }
        return claims.hasRole(Self.adminRole)
    }

    /// Ties telemetry to the signed-in user (opaque id only) and refreshes flags.
    public func didSignIn(userID: String) {
        analytics.setUserID(userID)
        telemetry.reloadFeatureFlags()
    }

    /// Clears per-user state that is not owned by the session store.
    public func didSignOut() async {
        analytics.setUserID(nil)
        badge.reset()
        await RemoteImageLoader.shared.removeAll()
    }

    // MARK: Factories (one fresh view model per screen)

    public func makeLoginViewModel() -> LoginViewModel {
        let identifiers = deviceIdentifiers
        return LoginViewModel(
            loginUseCase: LoginUseCase(repository: authRepository),
            analytics: analytics,
            deviceID: { identifiers.deviceID() }
        )
    }

    public func makeTaskListViewModel() -> TaskListViewModel {
        TaskListViewModel(repository: taskRepository, analytics: analytics)
    }

    public func makeTaskDetailViewModel(id: String) -> TaskDetailViewModel {
        TaskDetailViewModel(taskID: id, repository: taskRepository, analytics: analytics)
    }

    public func makeSearchViewModel() -> SearchViewModel {
        SearchViewModel(repository: searchRepository, clock: clock, analytics: analytics)
    }

    public func makeInboxViewModel() -> NotificationInboxViewModel {
        NotificationInboxViewModel(repository: notificationsRepository, badge: badge, analytics: analytics)
    }

    public func makeNotificationPreferencesViewModel() -> NotificationPreferencesViewModel {
        NotificationPreferencesViewModel(repository: notificationsRepository, push: push)
    }

    /// `taskID` scopes the collection to one task's attachments.
    public func makeMediaCollectionViewModel(taskID: String?) -> MediaCollectionViewModel {
        MediaCollectionViewModel(taskID: taskID, repository: mediaRepository, uploader: uploader, analytics: analytics)
    }

    public func makeMediaDetailViewModel(id: String) -> MediaDetailViewModel {
        MediaDetailViewModel(mediaID: id, repository: mediaRepository)
    }

    public var logoutUseCase: LogoutUseCase { LogoutUseCase(repository: authRepository) }
    public var restoreSessionUseCase: RestoreSessionUseCase { RestoreSessionUseCase(repository: authRepository) }

    private static func name(of environment: APIEnvironment) -> String {
        switch environment {
        case .local: "local"
        case .staging: "staging"
        case .production: "production"
        }
    }
}
