import AedstAppShared
import SwiftUI

@main
struct AedstApp_iOS: App {
    @State private var dependencies = AppDependencies()

    var body: some Scene {
        WindowGroup {
            RootView(dependencies: dependencies)
        }
    }
}
