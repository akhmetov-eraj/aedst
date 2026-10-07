"use client";

import { useCallback, useState } from "react";

/**
 * Keyset pagination for admin lists: the server only returns a *next*
 * token, so "previous" pops a stack of the tokens visited so far.
 */
export function useCursorPages() {
  const [stack, setStack] = useState<string[]>([]);
  const pageToken = stack[stack.length - 1] ?? "";

  const next = useCallback((token: string) => {
    if (token) {
      setStack((current) => [...current, token]);
    }
  }, []);
  const previous = useCallback(() => {
    setStack((current) => current.slice(0, -1));
  }, []);
  const reset = useCallback(() => setStack([]), []);

  return {
    pageToken,
    page: stack.length + 1,
    hasPrevious: stack.length > 0,
    next,
    previous,
    reset,
  };
}
