import AedstCore
import Foundation

/// The seam between `Presentation` and `Data` (the `tasks` module's
/// `application` boundary). Every call is scoped to the signed-in user by the
/// server; there is no user id parameter by design (see tasks.proto).
public protocol TaskRepository: Sendable {
    /// One page of the caller's tasks, newest first. `search` is a substring filter.
    func list(search: String, pageToken: String?) async -> Outcome<Page<TaskItem>>
    func get(id: String) async -> Outcome<TaskItem>
    func create(_ task: NewTask) async -> Outcome<TaskItem>
    func complete(id: String) async -> Outcome<TaskItem>
    func reopen(id: String) async -> Outcome<TaskItem>
    func delete(id: String) async -> Outcome<Void>
}

/// Validates and creates a task — the one place title rules live on the client
/// (the server enforces the same limits via Protovalidate).
public struct CreateTaskUseCase: UseCase {
    /// Same limit as `CreateTaskRequest.title` (max_len 200).
    public static let maxTitleLength = 200
    private let repository: any TaskRepository

    /// Uses `repository`.
    public init(repository: any TaskRepository) {
        self.repository = repository
    }

    public func callAsFunction(_ input: NewTask) async -> Outcome<TaskItem> {
        let title = input.title.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !title.isEmpty, title.count <= Self.maxTitleLength else {
            return .failure(.validation(fields: ["title"]))
        }
        var task = input
        task.title = title
        return await repository.create(task)
    }
}
