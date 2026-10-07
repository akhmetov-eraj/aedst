import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it } from "vitest";

import { TaskAttachments } from "@/modules/media/ui/TaskAttachments";
import {
  MediaKind,
  MediaService,
  MediaStatus,
  VariantKind,
} from "@/shared/api/connect/generated/aedst/media/v1/media_pb";
import { rpcHandler } from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.media;
const TASK_ID = "00000000-0000-4000-8000-000000000301";

const image = {
  id: "00000000-0000-4000-8000-000000000302",
  kind: MediaKind.IMAGE,
  status: MediaStatus.READY,
  filename: "photo.jpg",
  contentType: "image/jpeg",
  url: "https://storage.example/photo.jpg",
  width: 1600,
  height: 1200,
  variants: [
    {
      kind: VariantKind.THUMBNAIL,
      url: "https://storage.example/320.webp",
      width: 320,
      height: 240,
    },
    {
      kind: VariantKind.OPTIMIZED,
      url: "https://storage.example/1600.webp",
      width: 1600,
      height: 1200,
    },
  ],
};
const video = {
  id: "00000000-0000-4000-8000-000000000303",
  kind: MediaKind.VIDEO,
  status: MediaStatus.READY,
  filename: "clip.mp4",
  contentType: "video/mp4",
  url: "https://storage.example/clip.mp4",
  variants: [
    {
      kind: VariantKind.POSTER,
      url: "https://storage.example/poster.jpg",
      width: 640,
    },
  ],
};
const pdf = {
  id: "00000000-0000-4000-8000-000000000304",
  kind: MediaKind.DOCUMENT,
  status: MediaStatus.READY,
  filename: "notes.pdf",
  url: "https://storage.example/notes.pdf",
};
const processing = {
  id: "00000000-0000-4000-8000-000000000305",
  kind: MediaKind.AUDIO,
  status: MediaStatus.PROCESSING,
  filename: "memo.m4a",
};

describe("TaskAttachments", () => {
  it("lists the task's files and opens each kind with the right native player", async () => {
    let listedFor = "";
    server.use(
      rpcHandler(MediaService.method.listMedia, (input) => {
        listedFor = input.taskId;
        return { media: [image, video, pdf, processing] };
      }),
    );
    renderWithProviders(<TaskAttachments taskId={TASK_ID} />);

    const openImage = await screen.findByRole("button", {
      name: t.attachments.open.replace("{name}", "photo.jpg"),
    });
    expect(listedFor).toBe(TASK_ID);
    expect(within(openImage).getByRole("img")).toHaveAttribute(
      "srcset",
      expect.stringContaining("320w"),
    );
    expect(screen.getByText(t.attachments.processing)).toBeInTheDocument();

    await userEvent.click(openImage);
    const dialog = await screen.findByRole("dialog");
    expect(
      within(dialog).getByRole("img", { name: "photo.jpg" }),
    ).toHaveAttribute("src", "https://storage.example/1600.webp");
    await userEvent.keyboard("{Escape}");

    await userEvent.click(
      screen.getByRole("button", {
        name: t.attachments.open.replace("{name}", "clip.mp4"),
      }),
    );
    const player = (await screen.findByRole("dialog")).querySelector("video");
    expect(player).toHaveAttribute(
      "poster",
      "https://storage.example/poster.jpg",
    );
    expect(player?.querySelector("source")).toHaveAttribute(
      "src",
      "https://storage.example/clip.mp4",
    );
    await userEvent.keyboard("{Escape}");

    await userEvent.click(
      screen.getByRole("button", {
        name: t.attachments.open.replace("{name}", "notes.pdf"),
      }),
    );
    expect(
      await screen.findByRole("link", {
        name: t.player.open.replace("{name}", "notes.pdf"),
      }),
    ).toHaveAttribute("rel", "noopener noreferrer");
    await userEvent.keyboard("{Escape}");

    await userEvent.click(
      screen.getByRole("button", {
        name: t.attachments.open.replace("{name}", "memo.m4a"),
      }),
    );
    expect(await screen.findByText(t.player.processing)).toBeInTheDocument();
  }, 20_000);

  it("deletes an attachment", async () => {
    const deleted: string[] = [];
    server.use(
      rpcHandler(MediaService.method.listMedia, () => ({
        media: deleted.length ? [] : [pdf],
      })),
      rpcHandler(MediaService.method.deleteMedia, (input) => {
        deleted.push(input.id);
        return {};
      }),
    );
    renderWithProviders(<TaskAttachments taskId={TASK_ID} />);

    await userEvent.click(
      await screen.findByRole("button", {
        name: t.attachments.delete.replace("{name}", "notes.pdf"),
      }),
    );
    await waitFor(() => expect(deleted).toEqual([pdf.id]));
    expect(await screen.findByText(t.attachments.empty)).toBeInTheDocument();
  });
});
