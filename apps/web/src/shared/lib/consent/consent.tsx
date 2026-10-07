"use client";

import {
  createContext,
  use,
  useCallback,
  useMemo,
  useState,
  useTransition,
} from "react";
import { saveConsentAction } from "@/shared/lib/consent/consent-action";
import type { ConsentState } from "@/shared/lib/consent/consent-cookie";

type ConsentContextValue = {
  consent: ConsentState;
  grant: () => void;
  deny: () => void;
};

const ConsentContext = createContext<ConsentContextValue | null>(null);

/**
 * Whether the visitor allowed analytics/performance measurement. Seeded by
 * the server from the consent cookie (`initial`), so the very first render
 * already knows the answer. Measurement code must treat anything but
 * `granted` as off — opt-in, not opt-out.
 */
export function ConsentProvider({
  initial,
  children,
}: {
  initial: ConsentState;
  children: React.ReactNode;
}) {
  const [consent, setConsent] = useState<ConsentState>(initial);
  const [, startTransition] = useTransition();

  const choose = useCallback((choice: "granted" | "denied") => {
    // Optimistic: the UI (and measurement) follows immediately; the cookie
    // write is best effort — if it fails the choice still holds until reload.
    setConsent(choice);
    startTransition(async () => {
      try {
        await saveConsentAction(choice);
      } catch {
        // Kept in memory for this page view only.
      }
    });
  }, []);

  const value = useMemo<ConsentContextValue>(
    () => ({
      consent,
      grant: () => choose("granted"),
      deny: () => choose("denied"),
    }),
    [consent, choose],
  );

  return <ConsentContext value={value}>{children}</ConsentContext>;
}

export function useConsent(): ConsentContextValue {
  const ctx = use(ConsentContext);
  if (!ctx) {
    throw new Error("useConsent() must be called within <ConsentProvider>");
  }
  return ctx;
}
