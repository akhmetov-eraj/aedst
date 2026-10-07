import { splitHighlights } from "@/modules/search/lib/highlight";

/** Renders a server snippet with `<mark>` around matched terms — text only. */
export function HighlightedText({ text }: { text: string }) {
  return (
    <>
      {splitHighlights(text).map((segment, index) =>
        segment.match ? (
          <mark
            // biome-ignore lint/suspicious/noArrayIndexKey: segments are positional and never reorder
            key={index}
            className="rounded-sm bg-yellow-200 px-0.5 text-foreground dark:bg-yellow-800"
          >
            {segment.text}
          </mark>
        ) : (
          // biome-ignore lint/suspicious/noArrayIndexKey: segments are positional and never reorder
          <span key={index}>{segment.text}</span>
        ),
      )}
    </>
  );
}
