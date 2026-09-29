// Package transport implements the tasks module's ConnectRPC handlers. It
// depends only on modules/tasks/api — never on application/domain/
// infrastructure directly (see docs/modules.md). It never reads a user id
// from a request; every handler derives it from platform/auth.FromContext.
package transport

import (
	"context"
	"strconv"
	"time"

	"connectrpc.com/connect"
	"google.golang.org/protobuf/types/known/timestamppb"

	tasksv1 "github.com/aedst/server/gen/go/aedst/tasks/v1"
	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/httpserver"
	"github.com/aedst/server/internal/platform/idempotency"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"

	"github.com/aedst/server/internal/modules/tasks/api"
)

type Handler struct {
	service api.Service
}

func NewHandler(service api.Service) *Handler {
	return &Handler{service: service}
}

func (h *Handler) CreateTask(ctx context.Context, req *connect.Request[tasksv1.CreateTaskRequest]) (*connect.Response[tasksv1.CreateTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}

	key, _ := idempotency.KeyFromContext(ctx)
	task, svcErr := h.service.CreateTask(ctx, userID,
		req.Msg.GetTitle(), req.Msg.GetDescription(),
		fromProtoPriority(req.Msg.GetPriority()), fromProtoTimestamp(req.Msg.GetDueDate()), key)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.CreateTaskResponse{Task: toProtoTask(task)}), nil
}

func (h *Handler) GetTask(ctx context.Context, req *connect.Request[tasksv1.GetTaskRequest]) (*connect.Response[tasksv1.GetTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}
	id, err := ids.Parse(req.Msg.GetId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	task, svcErr := h.service.GetTask(ctx, userID, id)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.GetTaskResponse{Task: toProtoTask(task)}), nil
}

func (h *Handler) ListTasks(ctx context.Context, req *connect.Request[tasksv1.ListTasksRequest]) (*connect.Response[tasksv1.ListTasksResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}

	offset, err := decodePageToken(req.Msg.GetPageToken())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	statuses := make([]api.Status, 0, len(req.Msg.GetStatuses()))
	for _, s := range req.Msg.GetStatuses() {
		statuses = append(statuses, fromProtoStatus(s))
	}
	priorities := make([]api.Priority, 0, len(req.Msg.GetPriorities()))
	for _, p := range req.Msg.GetPriorities() {
		priorities = append(priorities, fromProtoPriority(p))
	}

	tasks, total, svcErr := h.service.ListTasks(ctx, api.ListParams{
		UserID:        userID,
		Statuses:      statuses,
		Priorities:    priorities,
		Search:        req.Msg.GetSearch(),
		SortBy:        fromProtoSortField(req.Msg.GetSortBy()),
		SortDirection: fromProtoSortDirection(req.Msg.GetSortDirection()),
		PageSize:      int(req.Msg.GetPageSize()),
		Offset:        offset,
	})
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}

	resp := &tasksv1.ListTasksResponse{
		Tasks:      make([]*tasksv1.Task, 0, len(tasks)),
		TotalCount: int32(total),
	}
	for _, t := range tasks {
		resp.Tasks = append(resp.Tasks, toProtoTask(t))
	}
	if next := offset + len(tasks); next < total {
		resp.NextPageToken = strconv.Itoa(next)
	}
	return connect.NewResponse(resp), nil
}

func (h *Handler) UpdateTask(ctx context.Context, req *connect.Request[tasksv1.UpdateTaskRequest]) (*connect.Response[tasksv1.UpdateTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}
	id, err := ids.Parse(req.Msg.GetId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	current, svcErr := h.service.GetTask(ctx, userID, id)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}

	title, description, priority, dueDate := current.Title, current.Description, current.Priority, current.DueDate
	paths := req.Msg.GetUpdateMask().GetPaths()
	if len(paths) == 0 {
		// No mask: apply every field the request carries — the common case
		// for a "save" button that always sends the whole form.
		title, description = req.Msg.GetTitle(), req.Msg.GetDescription()
		priority, dueDate = fromProtoPriority(req.Msg.GetPriority()), fromProtoTimestamp(req.Msg.GetDueDate())
	} else {
		for _, path := range paths {
			switch path {
			case "title":
				title = req.Msg.GetTitle()
			case "description":
				description = req.Msg.GetDescription()
			case "priority":
				priority = fromProtoPriority(req.Msg.GetPriority())
			case "due_date":
				dueDate = fromProtoTimestamp(req.Msg.GetDueDate())
			}
		}
	}

	task, svcErr := h.service.UpdateTask(ctx, userID, id, title, description, priority, dueDate)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.UpdateTaskResponse{Task: toProtoTask(task)}), nil
}

func (h *Handler) DeleteTask(ctx context.Context, req *connect.Request[tasksv1.DeleteTaskRequest]) (*connect.Response[tasksv1.DeleteTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}
	id, err := ids.Parse(req.Msg.GetId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	if svcErr := h.service.DeleteTask(ctx, userID, id); svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.DeleteTaskResponse{}), nil
}

func (h *Handler) CompleteTask(ctx context.Context, req *connect.Request[tasksv1.CompleteTaskRequest]) (*connect.Response[tasksv1.CompleteTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}
	id, err := ids.Parse(req.Msg.GetId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	task, svcErr := h.service.CompleteTask(ctx, userID, id)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.CompleteTaskResponse{Task: toProtoTask(task)}), nil
}

func (h *Handler) ReopenTask(ctx context.Context, req *connect.Request[tasksv1.ReopenTaskRequest]) (*connect.Response[tasksv1.ReopenTaskResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}
	id, err := ids.Parse(req.Msg.GetId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	task, svcErr := h.service.ReopenTask(ctx, userID, id)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.ReopenTaskResponse{Task: toProtoTask(task)}), nil
}

func (h *Handler) GetTaskStats(ctx context.Context, _ *connect.Request[tasksv1.GetTaskStatsRequest]) (*connect.Response[tasksv1.GetTaskStatsResponse], error) {
	userID, err := requireIdentity(ctx)
	if err != nil {
		return nil, err
	}

	stats, svcErr := h.service.GetStats(ctx, userID)
	if svcErr != nil {
		return nil, httpserver.ToConnectError(svcErr)
	}
	return connect.NewResponse(&tasksv1.GetTaskStatsResponse{
		Total:      int32(stats.Total),
		Todo:       int32(stats.Todo),
		InProgress: int32(stats.InProgress),
		Done:       int32(stats.Done),
		Overdue:    int32(stats.Overdue),
	}), nil
}

func requireIdentity(ctx context.Context) (ids.ID, error) {
	identity, ok := platformauth.FromContext(ctx)
	if !ok {
		// Unreachable in practice: no tasks procedure is in
		// publicProcedures, so AuthInterceptor has already rejected any
		// request that reaches here without a verified identity. Guarded
		// anyway rather than trusting that wiring never changes.
		return ids.Nil, httpserver.ToConnectError(sharederrors.Unauthorized("tasks", "missing identity"))
	}
	return identity.UserID, nil
}

func decodePageToken(token string) (int, error) {
	if token == "" {
		return 0, nil
	}
	offset, err := strconv.Atoi(token)
	if err != nil || offset < 0 {
		return 0, sharederrors.Validation("tasks.decodePageToken", "invalid page_token")
	}
	return offset, nil
}

func toProtoTask(t api.Task) *tasksv1.Task {
	return &tasksv1.Task{
		Id:          t.ID.String(),
		Title:       t.Title,
		Description: t.Description,
		Status:      toProtoStatus(t.Status),
		Priority:    toProtoPriority(t.Priority),
		DueDate:     toProtoTimestamp(t.DueDate),
		CreatedAt:   timestamppb.New(t.CreatedAt),
		UpdatedAt:   timestamppb.New(t.UpdatedAt),
		CompletedAt: toProtoTimestamp(t.CompletedAt),
		CreatedBy:   t.UserID.String(),
	}
}

func toProtoStatus(s api.Status) tasksv1.TaskStatus {
	switch s {
	case api.StatusTodo:
		return tasksv1.TaskStatus_TASK_STATUS_TODO
	case api.StatusInProgress:
		return tasksv1.TaskStatus_TASK_STATUS_IN_PROGRESS
	case api.StatusDone:
		return tasksv1.TaskStatus_TASK_STATUS_DONE
	default:
		return tasksv1.TaskStatus_TASK_STATUS_UNSPECIFIED
	}
}

func fromProtoStatus(s tasksv1.TaskStatus) api.Status {
	switch s {
	case tasksv1.TaskStatus_TASK_STATUS_TODO:
		return api.StatusTodo
	case tasksv1.TaskStatus_TASK_STATUS_IN_PROGRESS:
		return api.StatusInProgress
	case tasksv1.TaskStatus_TASK_STATUS_DONE:
		return api.StatusDone
	default:
		return ""
	}
}

func toProtoPriority(p api.Priority) tasksv1.TaskPriority {
	switch p {
	case api.PriorityLow:
		return tasksv1.TaskPriority_TASK_PRIORITY_LOW
	case api.PriorityMedium:
		return tasksv1.TaskPriority_TASK_PRIORITY_MEDIUM
	case api.PriorityHigh:
		return tasksv1.TaskPriority_TASK_PRIORITY_HIGH
	case api.PriorityUrgent:
		return tasksv1.TaskPriority_TASK_PRIORITY_URGENT
	default:
		return tasksv1.TaskPriority_TASK_PRIORITY_UNSPECIFIED
	}
}

func fromProtoPriority(p tasksv1.TaskPriority) api.Priority {
	switch p {
	case tasksv1.TaskPriority_TASK_PRIORITY_LOW:
		return api.PriorityLow
	case tasksv1.TaskPriority_TASK_PRIORITY_MEDIUM:
		return api.PriorityMedium
	case tasksv1.TaskPriority_TASK_PRIORITY_HIGH:
		return api.PriorityHigh
	case tasksv1.TaskPriority_TASK_PRIORITY_URGENT:
		return api.PriorityUrgent
	default:
		return ""
	}
}

func fromProtoSortField(f tasksv1.TaskSortField) string {
	if f == tasksv1.TaskSortField_TASK_SORT_FIELD_DUE_DATE {
		return "due_date"
	}
	return "created_at"
}

func fromProtoSortDirection(d tasksv1.SortDirection) string {
	if d == tasksv1.SortDirection_SORT_DIRECTION_ASC {
		return "asc"
	}
	return "desc"
}

func toProtoTimestamp(t *time.Time) *timestamppb.Timestamp {
	if t == nil {
		return nil
	}
	return timestamppb.New(*t)
}

func fromProtoTimestamp(t *timestamppb.Timestamp) *time.Time {
	if t == nil {
		return nil
	}
	tt := t.AsTime()
	return &tt
}
