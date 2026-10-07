# Спека: search/search

Тип: Query (+ проекция событий)
Модуль: search
Статус: Реализовано

## Контекст

Кросс-модульный поиск по собственному контенту пользователя. Модуль владеет
проекцией `search.documents` (Postgres FTS: `tsvector` с конфигурацией `simple`,
`websearch_to_tsquery`, `ts_rank_cd`, `ts_headline` с маркерами U+E000/U+E001;
`pg_trgm` для Suggest) и строит её только из integration events
(`tasks.v1.TaskCreated/TaskUpdated/TaskDeleted`, `media.v1.MediaReady/MediaDeleted`),
никогда не читая чужие схемы.

## Сценарии

**Happy path**

- Given задача создана/изменена
- When событие доставлено
- Then документ upsert-ится в inbox-транзакции, если версия события новее сохранённой.

- Given запрос `Search{query}`
- Then возвращаются только документы вызывающего, по убыванию ранга, keyset-страницами
  (курсор = (score, id), привязан к тексту запроса), `total_estimate` (cap 1000) — только на первой странице.

**Граничные случаи**

- События приходят не по порядку → побеждает большая версия; удаление оставляет tombstone
  (`deleted=true`, версия +1), старый снапшот его не «воскресит»; tombstone-ы чистит retention
  после `AEDST_RETENTION_INBOX_HOURS`.
- Токен от другого запроса или мусор → `INVALID_ARGUMENT`.
- Пустой/слишком длинный запрос → `INVALID_ARGUMENT`.

## Ошибки

| Условие | `sharederrors.Kind` | `connect.Code` |
| ------- | ------------------- | -------------- |
| Пустой запрос / префикс, плохой page_token | Validation | INVALID_ARGUMENT |
| Нет identity | Unauthorized | UNAUTHENTICATED |

## API

`aedst.search.v1.SearchService` — `Search`, `Suggest`.

## События

| Тип события | Версия | partition_key | Payload | Потребители |
| ----------- | ------ | -------------- | ------- | ----------- |
| tasks.v1.TaskCreated / TaskUpdated | 1 | task_id | `TaskSnapshot` (с `version`) | search |
| tasks.v1.TaskDeleted | 1 | task_id | `TaskDeletedEvent` (version = last+1) | search |
| media.v1.MediaReady / MediaDeleted | 1 | media_id | media/api/events.go | search, notifications |

## Кросс-модульные зависимости

Только типы событий из `tasks/api` и `media/api`.

## NFR

- Caller-scoped: `owner_id` в каждом запросе.
- Индексы: GIN(tsv), GIN(title gin_trgm_ops), (owner_id, updated_at, entity_id).
- Hot path: Search/Suggest (k6 `test/load/api.js`).
