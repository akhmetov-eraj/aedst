import Foundation

/// The `tasks` module's task on the client (`aedst.tasks.v1.Task` without the
/// wire format). Named `TaskItem` because `Task` is Swift Concurrency's type.
public struct TaskItem: Sendable, Equatable, Identifiable {
    /// Status is a state machine on the server: it only moves through
    /// CompleteTask / ReopenTask, never through UpdateTask.
    public enum Status: Sendable, Equatable {
        case todo
        case inProgress
        case done
    }

    /// Mirrors `aedst.tasks.v1.TaskPriority`.
    public enum Priority: Sendable, Equatable, CaseIterable {
        case low
        case medium
        case high
        case urgent
    }

    public let id: String
    public var title: String
    public var details: String
    public var status: Status
    public var priority: Priority
    public var dueDate: Date?
    public var createdAt: Date?
    public var completedAt: Date?
    /// Optimistic-concurrency version (send back as `expected_version`).
    public var version: Int32

    /// Memberwise initializer.
    public init(
        id: String,
        title: String,
        details: String = "",
        status: Status = .todo,
        priority: Priority = .medium,
        dueDate: Date? = nil,
        createdAt: Date? = nil,
        completedAt: Date? = nil,
        version: Int32 = 1
    ) {
        self.id = id
        self.title = title
        self.details = details
        self.status = status
        self.priority = priority
        self.dueDate = dueDate
        self.createdAt = createdAt
        self.completedAt = completedAt
        self.version = version
    }

    /// `true` when not done and the due date has passed.
    public func isOverdue(at now: Date) -> Bool {
        guard status != .done, let dueDate else { return false }
        return dueDate < now
    }
}

/// Input for `CreateTask`.
public struct NewTask: Sendable, Equatable {
    public var title: String
    public var details: String
    public var priority: TaskItem.Priority
    public var dueDate: Date?

    /// Memberwise initializer.
    public init(title: String, details: String = "", priority: TaskItem.Priority = .medium, dueDate: Date? = nil) {
        self.title = title
        self.details = details
        self.priority = priority
        self.dueDate = dueDate
    }
}
