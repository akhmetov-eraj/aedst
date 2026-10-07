# Спека: media/uploads-and-processing

Тип: Command + Query
Модуль: media
Статус: Реализовано

## Контекст

Загрузка файлов без прохождения байтов через API: presigned PUT в объектное
хранилище (`internal/platform/storage`: одна S3-совместимая реализация на
aws-sdk-go-v2 с пресетами s3 | minio | r2 + in-memory fake), затем
`CompleteUpload` и обработка River-job на отдельной очереди `media`
(можно вынести в отдельный worker-деплой: `AEDST_JOBS_QUEUES=media=4`).

## Сценарии

**Happy path**

- `CreateUpload{filename, content_type, size_bytes, sha256?, task_id?}` → проверка allow-list
  типов и лимита размера по виду, владение задачей через `tasks/api`, строка `PENDING_UPLOAD`,
  presigned PUT, подпись которого покрывает Content-Type, Content-Length и (если задан)
  `x-amz-checksum-sha256`.
- Клиент делает PUT напрямую в хранилище с заголовками из ответа.
- `CompleteUpload{id}` → `Head` (размер = заявленному), sniff первых 512 байт
  (`http.DetectContentType`) согласован с заявленным типом → guarded
  `pending_upload → processing` + `media.process` job в одной транзакции.
- Job: изображения — thumbnail 256 / preview 1280 / optimized 2560 (x/image/draw CatmullRom,
  перекодирование JPEG q75–85, PNG при прозрачности); видео — ffprobe + poster-кадр + thumbnail
  (если заданы `AEDST_MEDIA_FFMPEG_PATH`/`FFPROBE_PATH`, с таймаутом); аудио — длительность.
  Затем guarded `processing → ready` + `media.v1.MediaReady` в outbox.

**Граничные случаи**

- `CompleteUpload` до загрузки → `FAILED_PRECONDITION`; повторный → текущее состояние без второй обработки.
- Размер/тип не совпали → `FAILED` + удаление объекта.
- Битый файл / decompression bomb (> `AEDST_MEDIA_MAX_IMAGE_PIXELS`) → `FAILED`, job отменён.
- Хранилище не настроено (`AEDST_STORAGE_BUCKET` пуст) → все RPC `FAILED_PRECONDITION`.
- Брошенные `PENDING_UPLOAD` старше `AEDST_MEDIA_PENDING_UPLOAD_TTL_HOURS` удаляет retention
  (строки + job удаления объектов).
- `DeleteMedia` → строка и варианты удаляются, `media.v1.MediaDeleted` в outbox,
  объекты удаляет job `media.delete_objects`.

## Ошибки

| Условие | `sharederrors.Kind` | `connect.Code` |
| ------- | ------------------- | -------------- |
| Тип не в allow-list, превышен размер, плохое имя/sha256 | Validation | INVALID_ARGUMENT |
| Чужая/несуществующая задача или медиа | NotFound | NOT_FOUND |
| Файл ещё не загружен; хранилище не настроено | FailedPrecondition | FAILED_PRECONDITION |

## API

`aedst.media.v1.MediaService`.

## События

| Тип события | Версия | partition_key | Payload | Потребители |
| ----------- | ------ | -------------- | ------- | ----------- |
| media.v1.MediaReady | 1 | media_id | media/api/events.go | notifications, search |
| media.v1.MediaDeleted | 1 | media_id | media/api/events.go | search |

## Кросс-модульные зависимости

`tasks/api.Service.GetTask` — синхронная проверка владения задачей при прикреплении
(нужен ответ до выдачи URL, событие не подходит).

## NFR

- Безопасность: SVG/HTML не разрешены; ключи объектов генерирует сервер; имя файла санитизируется.
- Presigned URL TTL: upload 900 с, download 3600 с.
