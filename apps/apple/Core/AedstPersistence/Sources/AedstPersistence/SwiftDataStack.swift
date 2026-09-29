import SwiftData

/// One `ModelContainer` for every `@Model` local-cache type across feature
/// modules. Each feature registers its own models here instead of building
/// its own container, so the app has a single SwiftData store on disk.
public enum SwiftDataStack {
    public static func makeContainer(
        for models: [any PersistentModel.Type],
        inMemory: Bool = false
    ) throws -> ModelContainer {
        let schema = Schema(models)
        let configuration = ModelConfiguration(schema: schema, isStoredInMemoryOnly: inMemory)
        return try ModelContainer(for: schema, configurations: [configuration])
    }
}
