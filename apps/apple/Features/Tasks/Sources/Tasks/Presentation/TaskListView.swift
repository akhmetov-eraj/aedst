import AedstDesignSystem
import Perception
import SwiftUI

/// The task list. Selecting a row calls `onSelect` with the task id; the
/// composition root turns that into `TaskRoute.detail`.
public struct TaskListView: View {
    @State private var viewModel: TaskListViewModel
    private let onSelect: (String) -> Void

    /// `onSelect` receives the selected task's id.
    public init(viewModel: TaskListViewModel, onSelect: @escaping (String) -> Void) {
        self._viewModel = State(wrappedValue: viewModel)
        self.onSelect = onSelect
    }

    public var body: some View {
        WithPerceptionTracking {
            @Perception.Bindable var viewModel = viewModel

            List {
                if viewModel.hasLoaded, viewModel.tasks.isEmpty {
                    Text(String(localized: "tasksEmpty", bundle: .module))
                        .aedstFont(AedstTypography.body)
                        .foregroundStyle(AedstColor.mutedForeground)
                }
                ForEach(viewModel.tasks) { task in
                    Button {
                        onSelect(task.id)
                    } label: {
                        TaskRowView(task: task)
                    }
                    .buttonStyle(.plain)
                    .task { await viewModel.loadMoreIfNeeded(after: task) }
                }
                if viewModel.isLoading {
                    AedstProgressView(style: .circular)
                        .frame(maxWidth: .infinity)
                }
            }
            .refreshable { await viewModel.refresh() }
            .navigationTitle(String(localized: "tasksTitle", bundle: .module))
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        viewModel.isComposerPresented = true
                    } label: {
                        Label(String(localized: "tasksNew", bundle: .module), systemImage: "plus")
                    }
                }
            }
            .sheet(isPresented: $viewModel.isComposerPresented) {
                TaskComposerView(viewModel: viewModel)
            }
            .aedstErrorPresenter($viewModel.error) {
                Task { await viewModel.refresh() }
            }
            .task {
                if !viewModel.hasLoaded { await viewModel.refresh() }
            }
        }
    }
}

struct TaskRowView: View {
    let task: TaskItem

    var body: some View {
        HStack(spacing: AedstSpacing.md) {
            Image(systemName: task.status == .done ? "checkmark.circle.fill" : "circle")
                .foregroundStyle(task.status == .done ? AedstColor.success : AedstColor.mutedForeground)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: AedstSpacing.xxs) {
                Text(task.title)
                    .aedstFont(AedstTypography.body)
                    .strikethrough(task.status == .done)
                if let dueDate = task.dueDate {
                    Text(TaskFormatting.due(dueDate))
                        .aedstFont(AedstTypography.caption)
                        .foregroundStyle(task.isOverdue(at: Date()) ? AedstColor.destructive : AedstColor.mutedForeground)
                }
            }
            Spacer(minLength: 0)
            AedstBadge(TaskFormatting.priority(task.priority), variant: task.priority == .urgent ? .destructive : .secondary)
        }
        .contentShape(Rectangle())
        .accessibilityElement(children: .combine)
        .accessibilityValue(Text(TaskFormatting.status(task.status)))
    }
}

private struct TaskComposerView: View {
    let viewModel: TaskListViewModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        WithPerceptionTracking {
            @Perception.Bindable var viewModel = viewModel
            VStack(alignment: .leading, spacing: AedstSpacing.lg) {
                Text(String(localized: "tasksNew", bundle: .module))
                    .aedstFont(AedstTypography.title)
                    .accessibilityAddTraits(.isHeader)
                AedstTextField(String(localized: "tasksTitleField", bundle: .module), text: $viewModel.draftTitle)
                Picker(String(localized: "tasksPriority", bundle: .module), selection: $viewModel.draftPriority) {
                    ForEach(TaskItem.Priority.allCases, id: \.self) { priority in
                        Text(TaskFormatting.priority(priority)).tag(priority)
                    }
                }
                .pickerStyle(.segmented)
                HStack {
                    AedstButton(String(localized: "tasksCancel", bundle: .module), variant: .ghost) { dismiss() }
                    AedstButton(
                        String(localized: "tasksCreate", bundle: .module),
                        isLoading: viewModel.isSaving,
                        isDisabled: !viewModel.canSaveDraft
                    ) {
                        Task { await viewModel.saveDraft() }
                    }
                }
                Spacer()
            }
            .padding(AedstSpacing.xxl)
            .frame(minWidth: 320, minHeight: 280)
        }
    }
}

/// Localized display strings shared by the task screens.
enum TaskFormatting {
    static func priority(_ priority: TaskItem.Priority) -> String {
        switch priority {
        case .low: String(localized: "tasksPriorityLow", bundle: .module)
        case .medium: String(localized: "tasksPriorityMedium", bundle: .module)
        case .high: String(localized: "tasksPriorityHigh", bundle: .module)
        case .urgent: String(localized: "tasksPriorityUrgent", bundle: .module)
        }
    }

    static func status(_ status: TaskItem.Status) -> String {
        switch status {
        case .todo: String(localized: "tasksStatusTodo", bundle: .module)
        case .inProgress: String(localized: "tasksStatusInProgress", bundle: .module)
        case .done: String(localized: "tasksStatusDone", bundle: .module)
        }
    }

    static func due(_ date: Date) -> String {
        let formatted = date.formatted(date: .abbreviated, time: .omitted)
        return String(format: String(localized: "tasksDue", bundle: .module), formatted)
    }
}
