import AedstAnalytics
import AedstCore
import AedstPersistence
import Auth
import Foundation
import Perception
import SwiftUI

/// App-level state: who is signed in (and whether they are an admin), whether
/// the network is reachable, whether the biometric lock is covering the UI,
/// and the analytics-consent choice. One place, unit-testable without SwiftUI.
@MainActor
@Perceptible
public final class RootViewModel {
    public private(set) var session: Session?
    public private(set) var isOnline = true
    /// The lock screen must be shown (needs an unlock to dismiss).
    public private(set) var isLocked = false
    /// The app is not active: cover content so the app-switcher snapshot and
    /// screen recordings never show account data (only when app lock is on).
    public private(set) var isObscured = false
    public private(set) var isAppLockEnabled = false
    /// From the access token's roles claim — UX only, the server authorizes.
    public private(set) var isAdmin = false
    public private(set) var analyticsConsent: AnalyticsConsent = .unknown

    private let dependencies: AppDependencies
    private let logger = AedstLogger(category: "root")

    public init(dependencies: AppDependencies) {
        self.dependencies = dependencies
    }

    public var isSignedIn: Bool { session != nil }
    public var canOfferAppLock: Bool { dependencies.biometrics.isAvailable }
    public var showsLockScreen: Bool { isLocked || isObscured }

    /// Drives everything for the lifetime of the root view (`.task`): restores
    /// the session, then watches session events and reachability until cancelled.
    public func run() async {
        dependencies.startServices()
        await restore()
        if isSignedIn { await refreshSignedInServices() }
        await withTaskGroup(of: Void.self) { group in
            group.addTask { await self.observeSessionEvents() }
            group.addTask { await self.observeConnectivity() }
        }
    }

    /// Restores the persisted session without any network call, so the app
    /// opens signed-in — including offline or with an expired access token.
    func restore() async {
        if case .success(let restored) = await dependencies.restoreSessionUseCase(NoInput()) {
            session = restored
        }
        isAppLockEnabled = dependencies.appLockPreference.isEnabled
        analyticsConsent = dependencies.analyticsConsent
        isLocked = session != nil && lockConfigured
        if let session {
            dependencies.didSignIn(userID: session.userID)
            isAdmin = await dependencies.isAdmin()
        }
    }

    func observeSessionEvents() async {
        for await event in dependencies.sessionStore.events {
            handle(event)
        }
    }

    func observeConnectivity() async {
        for await online in dependencies.connectivity.updates {
            isOnline = online
        }
    }

    func handle(_ event: SessionEvent) {
        switch event {
        case .expired:
            session = nil
            isLocked = false
            isAdmin = false
            Task { await dependencies.didSignOut() }
        }
    }

    public func didSignIn(_ session: Session) {
        self.session = session
        dependencies.didSignIn(userID: session.userID)
        Task {
            isAdmin = await dependencies.isAdmin()
            await refreshSignedInServices()
        }
    }

    /// Unread badge + APNs re-registration (only if the user already allowed
    /// notifications — the permission prompt is an explicit user action).
    private func refreshSignedInServices() async {
        await dependencies.badge.refresh()
        await dependencies.push.refreshRegistrationIfAuthorized()
    }

    /// Asks for notification permission and registers with APNs.
    public func enablePushNotifications() async {
        await dependencies.push.enablePush()
    }

    public func signOut() async {
        // Unregister while the session is still valid (the RPC is authenticated).
        await dependencies.push.unregister()
        _ = await dependencies.logoutUseCase(NoInput())
        await dependencies.didSignOut()
        session = nil
        isAdmin = false
        isLocked = false
        isObscured = false
    }

    /// Persists and applies the analytics choice.
    public func setAnalytics(enabled: Bool) {
        let consent: AnalyticsConsent = enabled ? .granted : .denied
        do {
            try dependencies.setAnalyticsConsent(consent)
            analyticsConsent = consent
        } catch {
            logger.error("Could not persist the analytics consent", error: error)
        }
    }

    // MARK: App lock

    private var lockConfigured: Bool {
        dependencies.appLockPreference.isEnabled && dependencies.biometrics.isAvailable
    }

    public func handleScenePhase(_ phase: ScenePhase) {
        if phase == .active, isSignedIn {
            Task { await dependencies.badge.refresh() }
        }
        guard session != nil, lockConfigured else {
            isObscured = false
            return
        }
        switch phase {
        case .active:
            isObscured = false
        case .inactive:
            isObscured = true
        case .background:
            isObscured = true
            isLocked = true
        @unknown default:
            isObscured = true
        }
    }

    public func unlock() async {
        let reason = String(localized: "appUnlockReason", bundle: .module)
        if await dependencies.biometrics.authenticate(reason: reason) {
            isLocked = false
            isObscured = false
        }
    }

    /// Turning the lock on requires a successful authentication first, so a
    /// user can never lock themselves out with a biometric that does not work.
    public func setAppLock(enabled: Bool) async {
        if enabled {
            let reason = String(localized: "appUnlockReason", bundle: .module)
            guard await dependencies.biometrics.authenticate(reason: reason) else { return }
        }
        do {
            try dependencies.appLockPreference.setEnabled(enabled)
            isAppLockEnabled = enabled
        } catch {
            logger.error("Could not persist the app-lock preference", error: error)
        }
    }
}
