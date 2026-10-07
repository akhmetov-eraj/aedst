import { create } from "@bufbuild/protobuf";
import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { ImageViewer } from "@/modules/media/image/ImageViewer";
import { Thumbnail } from "@/modules/media/thumbnail/Thumbnail";

import {
  buildSrcSet,
  imageVariants,
  pickThumbnailUrl,
  playbackSources,
  posterUrl,
} from "@/modules/media/thumbnail/variants";
import {
  MediaKind,
  MediaSchema,
  VariantKind,
} from "@/shared/api/connect/generated/aedst/media/v1/media_pb";

const image = create(MediaSchema, {
  kind: MediaKind.IMAGE,
  url: "https://s3/original.jpg",
  contentType: "image/jpeg",
  variants: [
    { kind: VariantKind.OPTIMIZED, url: "https://s3/1600.webp", width: 1600 },
    { kind: VariantKind.THUMBNAIL, url: "https://s3/320.webp", width: 320 },
    { kind: VariantKind.PREVIEW, url: "https://s3/800.webp", width: 800 },
  ],
});

const video = create(MediaSchema, {
  kind: MediaKind.VIDEO,
  url: "https://s3/original.mov",
  contentType: "video/quicktime",
  variants: [
    { kind: VariantKind.POSTER, url: "https://s3/poster.jpg", width: 1280 },
    {
      kind: VariantKind.OPTIMIZED,
      url: "https://s3/720.mp4",
      contentType: "video/mp4",
    },
  ],
});

describe("variants", () => {
  it("orders image renditions by width and builds a srcset", () => {
    expect(imageVariants(image).map((v) => v.width)).toEqual([320, 800, 1600]);
    expect(buildSrcSet(imageVariants(image))).toBe(
      "https://s3/320.webp 320w, https://s3/800.webp 800w, https://s3/1600.webp 1600w",
    );
  });

  it("picks the smallest rendition that is sharp at 2x", () => {
    expect(pickThumbnailUrl(image, 120)).toBe("https://s3/320.webp");
    expect(pickThumbnailUrl(image, 300)).toBe("https://s3/800.webp");
    expect(pickThumbnailUrl(image, 2000)).toBe("https://s3/1600.webp");
  });

  it("uses the poster for video thumbnails and plays the optimized rendition first", () => {
    expect(pickThumbnailUrl(video, 120)).toBe("https://s3/poster.jpg");
    expect(posterUrl(video)).toBe("https://s3/poster.jpg");
    expect(playbackSources(video)).toEqual([
      { url: "https://s3/720.mp4", type: "video/mp4" },
      { url: "https://s3/original.mov", type: "video/quicktime" },
    ]);
  });

  it("falls back to the original image, or nothing", () => {
    const bare = create(MediaSchema, {
      kind: MediaKind.IMAGE,
      url: "https://s3/o.png",
    });
    expect(pickThumbnailUrl(bare, 100)).toBe("https://s3/o.png");
    expect(
      pickThumbnailUrl(create(MediaSchema, { kind: MediaKind.DOCUMENT }), 100),
    ).toBe("");
  });
});

describe("viewer and thumbnail components", () => {
  it("fall back to the original image and a file icon", () => {
    const original = create(MediaSchema, {
      kind: MediaKind.IMAGE,
      url: "https://s3/o.png",
    });
    render(<ImageViewer media={original} alt="o" />);
    expect(screen.getByRole("img", { name: "o" })).toHaveAttribute(
      "src",
      "https://s3/o.png",
    );
    render(
      <Thumbnail
        media={create(MediaSchema, { kind: MediaKind.DOCUMENT })}
        alt="doc.pdf"
      />,
    );
    expect(screen.getByText("doc.pdf")).toBeInTheDocument();
  });
});
