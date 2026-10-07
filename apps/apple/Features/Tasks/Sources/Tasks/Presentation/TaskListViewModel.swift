import AedstAnalytics
import AedstCore
import Foundation
import Perception

/// The signed-in user's tasks: first page on appear, keyset pagination on
/// scroll, pull-to-refresh, and a minimal "new task" form.
@MainActor
@Perceptible
public final class TaskListViewModel {
    public private(set) var tasks: [TaskItem] = []
    public private(set) var isLoading = false
    public private(set) var hasLoaded = false
    /// Bound to `aedstErrorPresenter`.
    public var error: AedstError?

    /// New-task sheet state.
    public var isComposerPresented = false
    public var draftTitle = ""
    public var draftPriority: TaskItem.Priority = .medium
    public private(set) var isSaving = false

    private var nextPageToken: String?
    private let repository: any TaskRepository
    private let createTask: CreateTaskUseCase
    private let analytics: AnalyticsClient

    /// Uses `repository` for reads and writes.
    public init(repository: any TaskRepository, analytics: AnalyticsClient = NoopAnalyticsClient()) {
        self.repository = repository
        self.createTask = CreateTaskUseCase(repository: repository)
        self.analytics = analytics
    }

    /// `true` while the draft can be submitted.
    public var canSaveDraft: Bool {
        !draftTitle.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !isSaving
    }

    /// Reloads the first page.
    public func refresh() async {
        nextPageToken = nil
        await loadPage(replacing: true)
    }

    /// Loads the next page when `task` is the last row shown.
    public func loadMoreIfNeeded(after task: TaskItem) async {
        guard task.id == tasks.last?.id, nextPageToken != nil, !isLoading else { return }
        await loadPage(replacing: false)
    }

    /// Creates the drafted task and inserts it at the top.
    public func saveDraft() async {
        guard canSaveDraft else { return }
        isSaving = true
        defer { isSaving = false }
        switch await createTask(NewTask(title: draftTitle, priority: draftPriority)) {
        case .success(let task):
            tasks.insert(task, at: 0)
            draftTitle = ""
            draftPriority = .medium
            isComposerPresented = false
            analytics.track(AnalyticsEvent(name: "task_created", parameters: ["priority": "\(task.priority)"]))
        case .failure(let failure):
            error = failure
        }
    }

    /// Reflects a change made on the detail screen without refetching.
    public func apply(_ task: TaskItem) {
        if let index = tasks.firstIndex(where: { $0.id == task.id }) {
            tasks[index] = task
        }
    }

    /// Removes a task deleted elsewhere.
    public func remove(id: String) {
        tasks.removeAll { $0.id == id }
    }

    private func loadPage(replacing: Bool) async {
        guard !isLoading else { return }
        isLoading = true
        defer {
            isLoading = false
            hasLoaded = true
        }
        switch await repository.list(search: "", pageToken: replacing ? nil : nextPageToken) {
        case .success(let page):
            tasks = replacing ? page.items : tasks + page.items
            nextPageToken = page.nextPageToken
        case .failure(let failure):
            error = failure
        }
    }
}
