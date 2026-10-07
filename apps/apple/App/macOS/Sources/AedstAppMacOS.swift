import AedstAppShared
import SwiftUI

@main
struct AedstAppMacOS: App {
    // The delegate owns `AppDependencies` so APNs / notification-tap callbacks
    // reach the same graph as the UI.
    @NSApplicationDelegateAdaptor(AedstAppDelegate.self) private var appDelegate

    var body: some Scene {
        WindowGroup {
            RootView(dependencies: appDelegate.dependencies)
                .frame(minWidth: 720, minHeight: 480)
        }
        .commands {
            SidebarCommands()
        }

        Settings {
            SettingsView()
        }
    }
}

/// macOS-only: iOS has no equivalent scene, hence `#if os(macOS)` guarding
/// nothing here — this whole file only builds into `App_macOS` per
/// `Project.swift`, unlike `AedstAppShared` which builds into both targets.
private struct SettingsView: View {
    var body: some View {
        Text("AEDST Preferences")
            .padding(40)
    }
}
