# Спека: admin/admin-service

Тип: Command + Query
Модуль: admin
Статус: Реализовано

## Контекст

Слой композиции для операторов. Собственная таблица — только журнал аудита
`admin.audit_log`; всё остальное читается и меняется через `api/`-пакеты
других модулей (tasks, notifications, auth, media). Роль `admin` обязательна
для каждого RPC: встроенная политика по умолчанию
`/aedst.admin.v1.AdminService/*=admin` (переопределяется тем же ключом в
`AEDST_ROLE_POLICY`), исполняется `httpserver.RoleInterceptor`.

## Сценарии

**Happy path**

- `DeleteTask{id, reason}` → в ОДНОЙ транзакции: `tasks/api.AdminService.DeleteTaskTx`
  (удаление + `tasks.v1.TaskDeleted` в outbox) и запись аудита.
- `BroadcastNotification{user_id?, title, body, link}` → в одной транзакции
  `notifications/api.Broadcaster.BroadcastTx` (уведомления + push-jobs) и запись аудита.
- `GetOverview` — счётчики модулей + лаг outbox + ожидающие River-jobs.
- `ListUsers` — каталог first-party пользователей (`auth/api.Directory`) + число задач.

**Граничные случаи**

- Ошибка мутации → откат, записи аудита нет.
- First-party auth выключен → `ListUsers` = `FAILED_PRECONDITION` (каталог живёт в IdP).
- Ссылка в broadcast только относительная (`/...`), иначе `INVALID_ARGUMENT` (open redirect).
- Вызов без роли `admin` → `PERMISSION_DENIED` до хендлера.

## Ошибки

| Условие | `sharederrors.Kind` | `connect.Code` |
| ------- | ------------------- | -------------- |
| Нет роли admin | — (RoleInterceptor) | PERMISSION_DENIED |
| reason < 3 символов, плохой токен/ссылка | Validation | INVALID_ARGUMENT |
| Задача не найдена | NotFound | NOT_FOUND |
| Каталог пользователей недоступен | FailedPrecondition | FAILED_PRECONDITION |

## API

`aedst.admin.v1.AdminService`.

## События

Собственных нет; мутации публикуют события модулей-владельцев (`tasks.v1.TaskDeleted`).

## Кросс-модульные зависимости

Сознательное исключение из правила «транзакции не пересекают модули»: admin
передаёт свою `pgx.Tx` в `*Tx`-методы `tasks/api` и `notifications/api`, чтобы
запись аудита и действие фиксировались атомарно (одна БД, разные схемы).

## NFR

- Retention аудита: `AEDST_RETENTION_AUDIT_DAYS` (365).
- Пагинация: keyset по `(created_at, id)` для аудита, задач и пользователей.
