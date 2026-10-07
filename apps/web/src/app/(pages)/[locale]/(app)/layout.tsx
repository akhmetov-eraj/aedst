import type { Metadata } from "next";
import { Suspense } from "react";

import { AppNav } from "@/app/shell/AppNav";
import { InstallPrompt } from "@/app/shell/InstallPrompt";
import { CurrentUser } from "@/modules/auth/ui/CurrentUser";
import { SignOutButton } from "@/modules/auth/ui/SignOutButton";
import { NotificationBellSection } from "@/modules/notifications/ui/NotificationBellSection";
import {
  CommandPalette,
  CommandPaletteTrigger,
} from "@/modules/search/ui/CommandPalette";
import { ErrorBoundary } from "@/shared/lib/errors/error-boundary";

// Signed-in pages are per-user and gated by `proxy.ts`; keep them out of
// search indexes explicitly rather than relying on them being empty.
export const metadata: Metadata = { robots: { index: false, follow: false } };

/**
 * The signed-in shell: identity, primary navigation, search (Ctrl/Cmd+K),
 * the notification bell and sign-out — composed here from feature modules,
 * which never import each other. Each widget sits in its own error boundary
 * so a failing bell cannot take the page down.
 */
export default function AppLayout({ children }: LayoutProps<"/[locale]">) {
  return (
    <div className="mx-auto flex w-full max-w-5xl flex-1 flex-col gap-6 p-4 sm:p-6">
      <header className="flex flex-col gap-3">
        <div className="flex flex-wrap items-center justify-between gap-3">
          <Suspense fallback={<span />}>
            <CurrentUser />
          </Suspense>
          <div className="flex items-center gap-2">
            <ErrorBoundary feature="search" fallback={null}>
              <CommandPaletteTrigger />
              <CommandPalette />
            </ErrorBoundary>
            <ErrorBoundary feature="notifications" fallback={null}>
              <Suspense fallback={<span className="size-9" />}>
                <NotificationBellSection />
              </Suspense>
            </ErrorBoundary>
            <SignOutButton />
          </div>
        </div>
        <AppNav />
        <InstallPrompt />
      </header>
      {children}
    </div>
  );
}
