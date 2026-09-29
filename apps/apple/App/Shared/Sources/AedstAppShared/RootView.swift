import AedstDesignSystem
import AedstNavigation
import Auth
import SwiftUI

/// The one `NavigationStack` shared by iOS and macOS — each app target's
/// `@main` scene wraps this in its own platform chrome (`TabView`/sidebar
/// on iOS, a resizable `WindowGroup` on macOS) but the destinations and the
/// `Router` are identical, same as `AppNavHost.kt` composing every
/// Android `feature/*`'s navigation entry point.
public struct RootView: View {
    @State private var router = Router()
    private let dependencies: AppDependencies

    public init(dependencies: AppDependencies) {
        self.dependencies = dependencies
    }

    public var body: some View {
        NavigationStack(path: $router.path) {
            StartView()
                .authDestinations {
                    LoginView(
                        viewModel: LoginViewModel(loginUseCase: LoginUseCase(repository: dependencies.authRepository)),
                        onLoginSucceeded: { _ in router.popToRoot() }
                    )
                }
        }
        .environment(router)
        .background(AedstColor.background)
    }
}

private struct StartView: View {
    @Environment(Router.self) private var router

    var body: some View {
        VStack(spacing: AedstSpacing.md) {
            Text("AEDST")
                .aedstFont(AedstTypography.largeTitle)
            AedstButton("Sign in") {
                router.push(AuthRoute.login)
            }
        }
        .padding(AedstSpacing.lg)
    }
}
