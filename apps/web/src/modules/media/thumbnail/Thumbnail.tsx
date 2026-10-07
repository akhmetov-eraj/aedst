import { File01Icon, Video01Icon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";

import {
  buildSrcSet,
  imageVariants,
  pickThumbnailUrl,
} from "@/modules/media/thumbnail/variants";
import {
  type Media,
  MediaKind,
} from "@/shared/api/connect/generated/aedst/media/v1/media_pb";

/**
 * Responsive thumbnail from the server's variants: `srcset` + `sizes` let the
 * browser choose the rendition for the slot and screen density. A plain
 * `<img>` rather than `next/image`: the variants are already resized by the
 * media pipeline, and presigned object-storage URLs expire, so routing them
 * through the Next image optimizer would only add a cache of dead links.
 */
export function Thumbnail({
  media,
  size = 96,
  alt,
}: {
  media: Media;
  /** Rendered CSS width/height in px. */
  size?: number;
  alt: string;
}) {
  const src = pickThumbnailUrl(media, size);
  if (!src) {
    return (
      <span
        className="flex items-center justify-center rounded-md bg-muted text-muted-foreground"
        style={{ width: size, height: size }}
      >
        <HugeiconsIcon icon={File01Icon} strokeWidth={2} className="size-6" />
        <span className="sr-only">{alt}</span>
      </span>
    );
  }
  const variants = imageVariants(media);
  return (
    <span className="relative inline-flex">
      {/* biome-ignore lint/performance/noImgElement: presigned, pre-resized variants with srcset; next/image would cache expiring URLs */}
      <img
        src={src}
        srcSet={variants.length > 1 ? buildSrcSet(variants) : undefined}
        sizes={`${size}px`}
        alt={alt}
        width={size}
        height={size}
        loading="lazy"
        decoding="async"
        className="rounded-md bg-muted object-cover"
        style={{ width: size, height: size }}
      />
      {media.kind === MediaKind.VIDEO && (
        <span className="absolute end-1 bottom-1 rounded bg-black/60 p-0.5 text-white">
          <HugeiconsIcon
            icon={Video01Icon}
            strokeWidth={2}
            className="size-3.5"
          />
        </span>
      )}
    </span>
  );
}
