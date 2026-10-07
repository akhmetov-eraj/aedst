# Спека: notifications/inbox-and-push

Тип: Command + Query
Модуль: notifications
Статус: Реализовано

## Контекст

Глобальный модуль уведомлений шаблона: in-app inbox (проекция чужих событий),
реестр push-устройств, пользовательские настройки и доставка push через Web
Push (VAPID), FCM HTTP v1 и APNs. Модуль никогда не вызывается синхронно ради
создания уведомления — он потребляет integration events (`tasks.v1.TaskCompleted`,
`tasks.v1.TaskDueSoon`, `media.v1.MediaReady`) в inbox-транзакции; единственная
синхронная точка записи — `api.Broadcaster` для admin-модуля.

## Сценарии

**Happy path**

- Given событие `tasks.v1.TaskCompleted` в outbox
- When relay доставляет его в шину
- Then в одной inbox-транзакции создаётся строка `notifications.notifications`,
  ставится River-job `notifications.push` и отметка в `platform.inbox`;
  job раскладывает доставку на `notifications.push_device` (по одному на активное
  устройство), каждый отправляет push провайдеру платформы.

- Given клиент получил токен FCM/APNs или PushSubscription
- When вызывает `RegisterPushDevice`
- Then устройство upsert-ится по `(platform, address)` и активируется.

**Граничные случаи**

- Повторная доставка события → `ON CONFLICT (source_event_id) DO NOTHING`, push не ставится повторно.
- Провайдер отвечает 404/410/`Unregistered`/`BadDeviceToken`/`UNREGISTERED` → устройство деактивируется, job успешен.
- 429/5xx → River ретраит только этот device-job.
- Провайдер не сконфигурирован → устройство пропускается (`push.ErrDisabled`).
- Настройки `push_enabled=false` или выключенный вид → in-app запись создаётся, push нет.
- Более 20 активных устройств → `INVALID_ARGUMENT`.
- Устройство перешло к другому аккаунту после fan-out → push не отправляется.

## Ошибки

| Условие | `sharederrors.Kind` | `connect.Code` |
| ------- | ------------------- | -------------- |
| Чужой/несуществующий id в MarkRead/UnregisterPushDevice | NotFound | NOT_FOUND |
| Невалидная подписка/токен, лимит устройств, чужой cursor | Validation | INVALID_ARGUMENT |
| Нет identity | Unauthorized | UNAUTHENTICATED |

## API

`aedst.notifications.v1.NotificationsService` (см. `api/proto/aedst/notifications/v1/notifications.proto`).

## События

| Тип события | Версия | partition_key | Payload | Потребители |
| ----------- | ------ | -------------- | ------- | ----------- |
| (потребляет) tasks.v1.TaskCompleted / TaskDueSoon | 1 | task_id | tasks/api/events.go | notifications |
| (потребляет) media.v1.MediaReady | 1 | media_id | media/api/events.go | notifications |

## Push payload (контракт клиентов)

- FCM: `data` = `{kind, title, body, link, notification_id}` + блок `notification`.
- APNs: `aps` = `{alert:{title,body}, badge: unread count, sound: default}`, рядом ключи `link`, `kind`, `notification_id`.
- Web Push: зашифрованный (RFC 8291) JSON `{title, body, link, kind, notification_id}`.

## Кросс-модульные зависимости

Только через `tasks/api` и `media/api` (типы событий). Admin вызывает `api.Broadcaster.BroadcastTx` в своей транзакции аудита.

## NFR

- Идемпотентность: проекции — через inbox + UNIQUE source_event_id; Register — upsert.
- Retention: уведомления `AEDST_RETENTION_NOTIFICATION_DAYS` (90), отключённые устройства 30 дней.
- Hot path: `ListNotifications`, `GetUnreadCount` (ограниченный счётчик, cap 1000).
