import { buildSrcSet, imageVariants } from "@/modules/media/thumbnail/variants";
import type { Media } from "@/shared/api/connect/generated/aedst/media/v1/media_pb";

/**
 * Full-size image view: the browser picks the rendition for the viewport
 * from the same variant srcset (falling back to the original), so a phone
 * never downloads the 4K original just to show it at 400 px.
 */
export function ImageViewer({ media, alt }: { media: Media; alt: string }) {
  const variants = imageVariants(media);
  const largest = variants[variants.length - 1]?.url;
  return (
    // biome-ignore lint/performance/noImgElement: presigned, pre-resized variants with srcset; next/image would cache expiring URLs
    <img
      src={largest || media.url}
      srcSet={variants.length > 0 ? buildSrcSet(variants) : undefined}
      sizes="(min-width: 1024px) 960px, 100vw"
      alt={alt}
      width={media.width || undefined}
      height={media.height || undefined}
      decoding="async"
      className="max-h-[75vh] w-full rounded-md object-contain"
    />
  );
}
