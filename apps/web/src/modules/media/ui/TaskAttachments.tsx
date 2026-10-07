"use client";

import {
  createConnectQueryKey,
  useMutation,
  useQuery,
} from "@connectrpc/connect-query";
import { Delete02Icon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import { useQueryClient } from "@tanstack/react-query";
import { useTranslations } from "next-intl";
import { useState } from "react";

import { MediaPlayer } from "@/modules/media/player/MediaPlayer";
import { Thumbnail } from "@/modules/media/thumbnail/Thumbnail";
import { Uploader } from "@/modules/media/uploader/Uploader";
import { MediaService } from "@/shared/api/connect/client";
import {
  type Media,
  MediaStatus,
} from "@/shared/api/connect/generated/aedst/media/v1/media_pb";
import { Button } from "@/shared/ui/button";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
} from "@/shared/ui/dialog";
import { Skeleton } from "@/shared/ui/skeleton";

const PROCESSING_POLL_MS = 3_000;

function isSettling(media: Media) {
  return (
    media.status === MediaStatus.PENDING_UPLOAD ||
    media.status === MediaStatus.PROCESSING
  );
}

/**
 * A task's attachments: list (`ListMedia` filtered by task), upload
 * (presigned PUT, attached through `task_id`), view, delete. Lives in the
 * media module and is composed onto the task page by `app/` — the tasks
 * module never imports media (feature boundaries). While anything is still
 * processing on the server, the list polls until variants are ready.
 */
export function TaskAttachments({ taskId }: { taskId: string }) {
  const t = useTranslations("media.attachments");
  const queryClient = useQueryClient();
  const [viewing, setViewing] = useState<Media | null>(null);

  const list = useQuery(
    MediaService.method.listMedia,
    { taskId, pageSize: 100 },
    {
      meta: { feature: "media" },
      refetchInterval: (query) =>
        query.state.data?.media.some(isSettling) ? PROCESSING_POLL_MS : false,
    },
  );
  const remove = useMutation(MediaService.method.deleteMedia, {
    meta: { feature: "media" },
    onSuccess: () =>
      queryClient.invalidateQueries({
        queryKey: createConnectQueryKey({
          schema: MediaService,
          cardinality: undefined,
        }),
      }),
  });

  const media = list.data?.media ?? [];

  return (
    <section
      aria-labelledby="attachments-heading"
      className="flex flex-col gap-3"
    >
      <h2 id="attachments-heading" className="font-medium text-lg">
        {t("title")}
      </h2>

      {list.isPending ? (
        <Skeleton className="h-24 w-full" role="status" aria-busy="true" />
      ) : list.isError ? (
        <div role="alert" className="flex items-center gap-2 text-sm">
          {t("loadError")}
          <Button variant="outline" size="sm" onClick={() => list.refetch()}>
            {t("retry")}
          </Button>
        </div>
      ) : media.length === 0 ? (
        <p className="text-muted-foreground text-sm">{t("empty")}</p>
      ) : (
        <ul className="grid grid-cols-[repeat(auto-fill,minmax(7rem,1fr))] gap-3">
          {media.map((item) => (
            <li key={item.id} className="flex flex-col gap-1">
              <button
                type="button"
                onClick={() => setViewing(item)}
                className="rounded-md focus-visible:ring-2 focus-visible:ring-ring"
                aria-label={t("open", { name: item.filename })}
              >
                <Thumbnail media={item} size={112} alt={item.filename} />
              </button>
              <div className="flex items-center justify-between gap-1">
                <span className="truncate text-xs" title={item.filename}>
                  {isSettling(item) ? t("processing") : item.filename}
                </span>
                <Button
                  variant="ghost"
                  size="icon-xs"
                  disabled={remove.isPending}
                  aria-label={t("delete", { name: item.filename })}
                  onClick={() => remove.mutate({ id: item.id })}
                >
                  <HugeiconsIcon icon={Delete02Icon} strokeWidth={2} />
                </Button>
              </div>
            </li>
          ))}
        </ul>
      )}

      <Uploader taskId={taskId} />

      <Dialog
        open={viewing !== null}
        onOpenChange={(open) => {
          if (!open) {
            setViewing(null);
          }
        }}
      >
        <DialogContent className="sm:max-w-4xl">
          <DialogHeader>
            <DialogTitle className="truncate">{viewing?.filename}</DialogTitle>
          </DialogHeader>
          {viewing && <MediaPlayer media={viewing} />}
        </DialogContent>
      </Dialog>
    </section>
  );
}
