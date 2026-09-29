-- name: CreateTask :one
INSERT INTO tasks.tasks (id, user_id, title, description, priority, due_date, created_at, updated_at)
VALUES ($1, $2, $3, $4, $5, $6, $7, $7)
RETURNING id, user_id, title, description, status, priority, due_date, completed_at, created_at, updated_at;

-- name: FindTaskByIDAndUserID :one
SELECT id, user_id, title, description, status, priority, due_date, completed_at, created_at, updated_at
FROM tasks.tasks
WHERE id = $1 AND user_id = $2;

-- name: UpdateTask :one
-- Only title/description/priority/due_date — status is never set here, see
-- SetTaskStatus (tasks.proto's UpdateTask doc comment explains why).
UPDATE tasks.tasks
SET title = $3, description = $4, priority = $5, due_date = $6, updated_at = $7
WHERE id = $1 AND user_id = $2
RETURNING id, user_id, title, description, status, priority, due_date, completed_at, created_at, updated_at;

-- name: SetTaskStatus :one
-- Backs both CompleteTask (status='done', completed_at=now) and ReopenTask
-- (status='todo', completed_at=NULL) — same shape, different args, so one
-- query instead of two near-identical ones.
UPDATE tasks.tasks
SET status = $3, completed_at = $4, updated_at = $5
WHERE id = $1 AND user_id = $2
RETURNING id, user_id, title, description, status, priority, due_date, completed_at, created_at, updated_at;

-- name: DeleteTask :execrows
DELETE FROM tasks.tasks
WHERE id = $1 AND user_id = $2;

-- name: ListTasks :many
-- Dynamic filter/sort via CASE/cardinality rather than building SQL in Go —
-- keeps this the single source of truth for the query (sqlc still
-- type-checks it) instead of an ORM-shaped query builder. `count(*) OVER ()`
-- gives the filtered total in the same round trip ListTasksResponse needs,
-- instead of a second COUNT query.
SELECT id, user_id, title, description, status, priority, due_date, completed_at, created_at, updated_at,
       count(*) OVER () AS total_count
FROM tasks.tasks
WHERE user_id = sqlc.arg(user_id)
  AND (cardinality(sqlc.arg(statuses)::tasks.status[]) = 0 OR status = ANY (sqlc.arg(statuses)::tasks.status[]))
  AND (cardinality(sqlc.arg(priorities)::tasks.priority[]) = 0 OR priority = ANY (sqlc.arg(priorities)::tasks.priority[]))
  AND (sqlc.arg(search)::text = '' OR title ILIKE '%' || sqlc.arg(search) || '%' OR description ILIKE '%' || sqlc.arg(search) || '%')
ORDER BY
    CASE WHEN sqlc.arg(sort_by)::text = 'due_date' AND sqlc.arg(sort_direction)::text = 'asc' THEN due_date END ASC NULLS FIRST,
    CASE WHEN sqlc.arg(sort_by)::text = 'due_date' AND sqlc.arg(sort_direction)::text = 'desc' THEN due_date END DESC NULLS LAST,
    CASE WHEN sqlc.arg(sort_by)::text = 'created_at' AND sqlc.arg(sort_direction)::text = 'asc' THEN created_at END ASC,
    created_at DESC
LIMIT sqlc.arg(page_limit) OFFSET sqlc.arg(page_offset);

-- name: GetTaskStats :one
SELECT
    count(*)                                                                                AS total,
    count(*) FILTER (WHERE status = 'todo')                                                 AS todo,
    count(*) FILTER (WHERE status = 'in_progress')                                           AS in_progress,
    count(*) FILTER (WHERE status = 'done')                                                   AS done,
    count(*) FILTER (WHERE status != 'done' AND due_date IS NOT NULL AND due_date < now())    AS overdue
FROM tasks.tasks
WHERE user_id = $1;
