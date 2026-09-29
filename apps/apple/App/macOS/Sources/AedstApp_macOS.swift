import AedstAppShared
import SwiftUI

@main
struct AedstApp_macOS: App {
    @State private var dependencies = AppDependencies()

    var body: some Scene {
        WindowGroup {
            RootView(dependencies: dependencies)
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
/// `project.yml`, unlike `AedstAppShared` which builds into both targets.
private struct SettingsView: View {
    var body: some View {
        Text("AEDST Preferences")
            .padding(40)
    }
}
