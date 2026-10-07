import Admin
import AedstDesignSystem
import AedstNavigation
import Auth
import Media
import Notifications
import Perception
import Search
import SwiftUI
import Tasks

/// The one navigation stack shared by iOS and macOS — each app target's
/// `@main` scene wraps this in its own platform chrome but the destinations
/// and the `Router` are identical, same as `AppNavHost.kt` composing every
/// Android `feature/*`'s navigation entry point. This is the only place that
/// knows every feature: cross-feature wiring (task detail → media attachments,
/// search/notification links → routes) happens here.
public struct RootView: View {
    @State private var router = Router()
    @State private var model: RootViewModel
    @Environment(\.scenePhase) private var scenePhase
    private let dependencies: AppDependencies

    @MainActor
    public init(dependencies: AppDependencies) {
        self.dependencies = dependencies
        self._model = State(wrappedValue: RootViewModel(dependencies: dependencies))
    }

    public var body: some View {
        WithPerceptionTracking {
            ZStack {
                AedstNavigationStack(router: router) {
                    StartView(model: model, router: router, badge: dependencies.badge)
                        .authDestinations {
                            LoginView(
                                viewModel: dependencies.makeLoginViewModel(),
                                onLoginSucceeded: { session in
                                    model.didSignIn(session)
                                    router.popToRoot()
                                }
                            )
                        }
                        .modifier(FeatureDestinations(dependencies: dependencies, router: router, open: open))
                }

                VStack(spacing: 0) {
                    if !model.isOnline {
                        OfflineBanner()
                            .transition(.move(edge: .top).combined(with: .opacity))
                    }
                    Spacer(minLength: 0)
                }
                .aedstAnimation(.easeOut(duration: AedstMotion.durationNormal), value: model.isOnline)

                if model.showsLockScreen {
                    LockView(model: model)
                }
            }
            .background(AedstColor.background)
            .task { await model.run() }
            .task {
                for await link in dependencies.linkInbox.links {
                    open(link)
                }
            }
            .onChange(of: scenePhase) { model.handleScenePhase($0) }
            .onChange(of: model.session) { session in
                if session == nil { router.popToRoot() }
            }
            .onOpenURL { url in
                guard let link = DeepLink(url: url) else { return }
                handle(link)
            }
        }
    }

    private func handle(_ link: DeepLink) {
        switch link {
        case .login:
            guard !model.isSignedIn else { return }
            router.replace(with: AuthRoute.login)
        }
    }

    /// Routes a relative in-app link (notification, search hit, push payload).
    /// Unknown links and links to areas the user cannot open are ignored.
    private func open(_ path: String) {
        guard model.isSignedIn, let link = AppLink(path: path) else { return }
        dependencies.analytics.addBreadcrumb("open \(path)", category: "navigation")
        switch link {
        case .tasks: router.push(TaskRoute.list)
        case .task(let id): router.push(TaskRoute.detail(id: id))
        case .media: router.push(MediaRoute.library)
        case .mediaItem(let id): router.push(MediaRoute.detail(id: id))
        case .notifications: router.push(NotificationsRoute.inbox)
        case .search: router.push(SearchRoute.search)
        case .admin:
            if model.isAdmin { router.push(AdminRoute.dashboard) }
        }
    }
}

/// Every signed-in feature's destinations, composed in one place.
private struct FeatureDestinations: ViewModifier {
    let dependencies: AppDependencies
    let router: Router
    let open: (String) -> Void

    func body(content: Content) -> some View {
        content
            .tasksDestinations {
                TaskListView(viewModel: dependencies.makeTaskListViewModel()) { id in
                    router.push(TaskRoute.detail(id: id))
                }
            } makeDetail: { id in
                TaskDetailView(
                    viewModel: dependencies.makeTaskDetailViewModel(id: id),
                    onDeleted: { router.pop() }
                ) { taskID in
                    // Media attachments slot: Tasks never imports Media.
                    TaskAttachmentsView(viewModel: dependencies.makeMediaCollectionViewModel(taskID: taskID)) { mediaID in
                        router.push(MediaRoute.detail(id: mediaID))
                    }
                }
            }
            .searchDestinations {
                SearchView(viewModel: dependencies.makeSearchViewModel(), onOpenLink: open)
            }
            .notificationsDestinations {
                NotificationInboxView(
                    viewModel: dependencies.makeInboxViewModel(),
                    onOpenLink: open,
                    onOpenPreferences: { router.push(NotificationsRoute.preferences) }
                )
            } makePreferences: {
                NotificationPreferencesView(viewModel: dependencies.makeNotificationPreferencesViewModel())
            }
            .mediaDestinations {
                MediaLibraryView(viewModel: dependencies.makeMediaCollectionViewModel(taskID: nil)) { id in
                    router.push(MediaRoute.detail(id: id))
                }
            } makeDetail: { id in
                MediaDetailView(viewModel: dependencies.makeMediaDetailViewModel(id: id)) { router.pop() }
            }
            .adminDestinations { route in
                AdminScreen(route: route, dependencies: dependencies, router: router)
            }
    }
}

private struct AdminScreen: View {
    let route: AdminRoute
    let dependencies: AppDependencies
    let router: Router

    var body: some View {
        let repository = dependencies.adminRepository
        switch route {
        case .dashboard:
            AdminDashboardView(viewModel: AdminDashboardViewModel(repository: repository)) { router.push($0) }
        case .users:
            AdminUsersView(viewModel: AdminUsersViewModel(repository: repository))
        case .tasks:
            AdminTasksView(viewModel: AdminTasksViewModel(repository: repository, analytics: dependencies.analytics))
        case .broadcast:
            AdminBroadcastView(viewModel: AdminBroadcastViewModel(repository: repository))
        case .auditLog:
            AdminAuditLogView(viewModel: AdminAuditLogViewModel(repository: repository))
        }
    }
}

private struct StartView: View {
    let model: RootViewModel
    let router: Router
    let badge: UnreadBadgeModel

    var body: some View {
        WithPerceptionTracking {
            if model.isSignedIn {
                HomeView(model: model, router: router, badge: badge)
            } else {
                VStack(spacing: AedstSpacing.lg) {
                    Text("AEDST")
                        .aedstFont(AedstTypography.largeTitle)
                        .accessibilityAddTraits(.isHeader)
                    AedstButton(String(localized: "appSignIn", bundle: .module)) {
                        router.push(AuthRoute.login)
                    }
                }
                .padding(AedstSpacing.xxl)
                .frame(maxWidth: 480)
            }
        }
    }
}

/// Signed-in home: one entry per feature, plus account settings.
private struct HomeView: View {
    let model: RootViewModel
    let router: Router
    let badge: UnreadBadgeModel

    var body: some View {
        WithPerceptionTracking {
            // Read observable state here (inside tracking), not inside escaping closures.
            let isAppLockEnabled = model.isAppLockEnabled
            let analyticsEnabled = model.analyticsConsent == .granted
            List {
                Section {
                    entry("appHomeTasks", "checklist") { router.push(TaskRoute.list) }
                    entry("appHomeSearch", "magnifyingglass") { router.push(SearchRoute.search) }
                    Button {
                        router.push(NotificationsRoute.inbox)
                    } label: {
                        HStack {
                            Label(String(localized: "appHomeNotifications", bundle: .module), systemImage: "bell")
                            Spacer()
                            if badge.unread > 0 {
                                AedstBadge("\(badge.unread)")
                                    .accessibilityLabel(
                                        Text(String(format: String(localized: "appUnreadCount", bundle: .module), badge.unread))
                                    )
                            }
                        }
                    }
                    entry("appHomeFiles", "photo.on.rectangle") { router.push(MediaRoute.library) }
                    if model.isAdmin {
                        entry("appHomeAdmin", "shield") { router.push(AdminRoute.dashboard) }
                    }
                }

                Section(String(localized: "appSettings", bundle: .module)) {
                    Button(String(localized: "appEnableNotifications", bundle: .module)) {
                        Task { await model.enablePushNotifications() }
                    }
                    Toggle(
                        String(localized: "appShareAnalytics", bundle: .module),
                        isOn: Binding(get: { analyticsEnabled }, set: { model.setAnalytics(enabled: $0) })
                    )
                    if model.canOfferAppLock {
                        Toggle(
                            String(localized: "appRequireBiometrics", bundle: .module),
                            isOn: Binding(
                                get: { isAppLockEnabled },
                                set: { enabled in Task { await model.setAppLock(enabled: enabled) } }
                            )
                        )
                    }
                    Button(String(localized: "appSignOut", bundle: .module), role: .destructive) {
                        Task { await model.signOut() }
                    }
                }
            }
            .navigationTitle("AEDST")
        }
    }

    private func entry(_ key: String.LocalizationValue, _ icon: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Label(String(localized: key, bundle: .module), systemImage: icon)
        }
    }
}

private struct OfflineBanner: View {
    var body: some View {
        HStack(spacing: AedstSpacing.xs) {
            Image(systemName: "wifi.slash")
            Text(String(localized: "appOffline", bundle: .module))
                .aedstFont(AedstTypography.caption)
            Spacer(minLength: 0)
        }
        .padding(AedstSpacing.sm)
        .frame(maxWidth: .infinity)
        .background(AedstColor.warning)
        .foregroundStyle(AedstColor.warningForeground)
        .accessibilityElement(children: .combine)
    }
}

private struct LockView: View {
    let model: RootViewModel

    var body: some View {
        WithPerceptionTracking {
            VStack(spacing: AedstSpacing.lg) {
                Image(systemName: "lock.fill")
                    .font(.largeTitle)
                    .accessibilityHidden(true)
                Text(String(localized: "appLocked", bundle: .module))
                    .aedstFont(AedstTypography.title)
                    .accessibilityAddTraits(.isHeader)
                if model.isLocked {
                    AedstButton(String(localized: "appUnlock", bundle: .module)) {
                        Task { await model.unlock() }
                    }
                    .frame(maxWidth: 320)
                }
            }
            .padding(AedstSpacing.xxl)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(AedstColor.background)
            .accessibilityAddTraits(.isModal)
            // Prompt as soon as the lock appears (and again if it re-locks).
            .task(id: model.isLocked) {
                if model.isLocked { await model.unlock() }
            }
        }
    }
}
