import {
  type Media,
  MediaKind,
  type Variant,
  VariantKind,
} from "@/shared/api/connect/generated/aedst/media/v1/media_pb";

/**
 * Picking renditions from `Media.variants` (thumbnail/preview/optimized/
 * poster, each with real pixel dimensions). URLs are short-lived presigned
 * GETs, so they come from the latest `GetMedia`/`ListMedia` response and are
 * never persisted.
 */

const IMAGE_VARIANTS = new Set([
  VariantKind.THUMBNAIL,
  VariantKind.PREVIEW,
  VariantKind.OPTIMIZED,
]);

function byWidth(a: Variant, b: Variant) {
  return a.width - b.width;
}

/** Image renditions with a known width, smallest first. */
export function imageVariants(media: Media): Variant[] {
  return media.variants
    .filter((variant) => IMAGE_VARIANTS.has(variant.kind) && variant.width > 0)
    .sort(byWidth);
}

/** `srcset` for `<img>` (`url 320w, url 1280w`); empty when none have widths. */
export function buildSrcSet(variants: readonly Variant[]): string {
  return variants
    .map((variant) => `${variant.url} ${variant.width}w`)
    .join(", ");
}

/** The best single URL for a thumbnail slot of `targetWidth` CSS px (DPR 2). */
export function pickThumbnailUrl(media: Media, targetWidth: number): string {
  const variants = imageVariants(media);
  const wanted = targetWidth * 2;
  const fit = variants.find((variant) => variant.width >= wanted);
  const poster = media.variants.find((v) => v.kind === VariantKind.POSTER);
  if (fit) {
    return fit.url;
  }
  if (variants.length > 0) {
    return variants[variants.length - 1]?.url ?? "";
  }
  if (poster) {
    return poster.url;
  }
  return media.kind === MediaKind.IMAGE ? media.url : "";
}

/** Poster frame for video. */
export function posterUrl(media: Media): string | undefined {
  return (
    media.variants.find((variant) => variant.kind === VariantKind.POSTER)
      ?.url || undefined
  );
}

/**
 * Playback sources, best first: the server's optimized rendition, then the
 * original. The browser picks the first type it can play.
 */
export function playbackSources(media: Media): { url: string; type: string }[] {
  const optimized = media.variants
    .filter((variant) => variant.kind === VariantKind.OPTIMIZED && variant.url)
    .map((variant) => ({ url: variant.url, type: variant.contentType }));
  const original = media.url
    ? [{ url: media.url, type: media.contentType }]
    : [];
  return [...optimized, ...original];
}
