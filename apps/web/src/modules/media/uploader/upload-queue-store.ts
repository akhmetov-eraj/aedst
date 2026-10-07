"use client";

import { create } from "zustand";

/**
 * The upload queue — client UI state (progress bars, cancel/retry), shared
 * between the uploader and anything that shows uploads in flight. The
 * *uploaded media* is server state and lives in TanStack Query; this store
 * only knows about bytes that are still on their way.
 */
type UploadStatus =
  | "queued"
  | "compressing"
  | "uploading"
  | "finalizing"
  | "done"
  | "failed"
  | "cancelled";

export type UploadItem = {
  id: string;
  file: File;
  taskId?: string;
  status: UploadStatus;
  /** 0..1 */
  progress: number;
  /** Translation key under `media.uploader.errors`. */
  errorKey?: string;
  mediaId?: string;
  compressed?: boolean;
};

type UploadQueueState = {
  items: UploadItem[];
  add: (files: File[], taskId?: string) => UploadItem[];
  update: (id: string, patch: Partial<UploadItem>) => void;
  remove: (id: string) => void;
  clearFinished: () => void;
};

export const useUploadQueue = create<UploadQueueState>()((set) => ({
  items: [],
  add: (files, taskId) => {
    const added = files.map<UploadItem>((file) => ({
      id: crypto.randomUUID(),
      file,
      taskId,
      status: "queued",
      progress: 0,
    }));
    set((state) => ({ items: [...state.items, ...added] }));
    return added;
  },
  update: (id, patch) =>
    set((state) => ({
      items: state.items.map((item) =>
        item.id === id ? { ...item, ...patch } : item,
      ),
    })),
  remove: (id) =>
    set((state) => ({ items: state.items.filter((item) => item.id !== id) })),
  clearFinished: () =>
    set((state) => ({
      items: state.items.filter(
        (item) => item.status !== "done" && item.status !== "cancelled",
      ),
    })),
}));

/** AbortControllers are not state; they live beside the store, by item id. */
export const uploadControllers = new Map<string, AbortController>();
