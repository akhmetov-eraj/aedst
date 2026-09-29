"use client";

import * as Sentry from "@sentry/nextjs";
import { useEffect } from "react";

// The final fallback — catches errors the root layout itself throws, which
// `(pages)/[locale]/error.tsx` can't (a segment's error boundary never
// covers its own parent layout). Next.js requires this file to render its
// own complete `<html>`/`<body>`; nothing above it in the tree can be
// trusted to still be working, including next-intl's provider, so this
// stays translation-free like `app/not-found.tsx`.
export default function GlobalError({
  error,
}: {
  error: Error & { digest?: string };
}) {
  useEffect(() => {
    Sentry.captureException(error);
  }, [error]);

  return (
    <html lang="en">
      <body className="flex min-h-screen flex-col items-center justify-center gap-4 px-4 text-center font-sans">
        <p className="text-sm font-medium text-neutral-500">Error</p>
        <h1 className="text-2xl font-semibold text-neutral-900">
          Something went wrong
        </h1>
        <p className="max-w-md text-sm text-neutral-500">
          We've been notified and are looking into it.
        </p>
        <a
          href="/"
          className="text-sm font-medium text-neutral-900 underline underline-offset-4"
        >
          Go home
        </a>
      </body>
    </html>
  );
}
