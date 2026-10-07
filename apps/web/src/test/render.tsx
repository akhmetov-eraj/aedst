import { TransportProvider } from "@connectrpc/connect-query";
import { QueryClientProvider } from "@tanstack/react-query";
import { type RenderOptions, render } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { NuqsTestingAdapter } from "nuqs/adapters/testing";
import type { ReactElement } from "react";

import commonMessages from "@/app/config/i18n/common/en.json";
import adminMessages from "@/modules/admin/i18n/en.json";
import authMessages from "@/modules/auth/i18n/en.json";
import homeMessages from "@/modules/home/i18n/en.json";
import mediaMessages from "@/modules/media/i18n/en.json";
import notificationsMessages from "@/modules/notifications/i18n/en.json";
import searchMessages from "@/modules/search/i18n/en.json";
import tasksMessages from "@/modules/tasks/i18n/en.json";
import { createBrowserTransport } from "@/shared/api/connect/client";
import { AuthProvider } from "@/shared/lib/auth/auth-provider";
import {
  makeQueryClient,
  type QueryClientCallbacks,
} from "@/shared/lib/query-client";
import { TEST_RPC_BASE_URL } from "@/test/mocks/connect";

export const messages = {
  common: commonMessages,
  home: homeMessages,
  auth: authMessages,
  tasks: tasksMessages,
  search: searchMessages,
  notifications: notificationsMessages,
  media: mediaMessages,
  admin: adminMessages,
};

type Options = Omit<RenderOptions, "wrapper"> & {
  /** Initial URL search string for nuqs, e.g. `?status=done`. */
  searchParams?: string;
  initialUserId?: string | null;
  initialRoles?: string[];
  /** Keep prefetched/hydrated data fresh so no client refetch happens on mount. */
  staleTime?: number;
  callbacks?: QueryClientCallbacks;
  onUrlUpdate?: (event: { queryString: string }) => void;
};

/**
 * Renders with the same provider stack the real layout builds — i18n,
 * Connect transport (pointed at the MSW-handled test origin), TanStack Query
 * (no retries, no stale time, so tests see real fetches deterministically),
 * URL state and auth — minus anything needing a running Next server.
 */
export function renderWithProviders(
  ui: ReactElement,
  {
    searchParams = "",
    initialUserId = null,
    initialRoles,
    staleTime = 0,
    callbacks,
    onUrlUpdate,
    ...options
  }: Options = {},
) {
  const queryClient = makeQueryClient(callbacks);
  queryClient.setDefaultOptions({
    queries: { retry: false, staleTime, gcTime: Number.POSITIVE_INFINITY },
    mutations: { retry: false },
  });
  const transport = createBrowserTransport(TEST_RPC_BASE_URL);

  const result = render(ui, {
    ...options,
    wrapper: ({ children }) => (
      <NextIntlClientProvider locale="en" messages={messages}>
        <TransportProvider transport={transport}>
          <QueryClientProvider client={queryClient}>
            <NuqsTestingAdapter
              hasMemory
              searchParams={searchParams}
              onUrlUpdate={onUrlUpdate}
            >
              <AuthProvider
                initialUserId={initialUserId}
                initialRoles={initialRoles}
              >
                {children}
              </AuthProvider>
            </NuqsTestingAdapter>
          </QueryClientProvider>
        </TransportProvider>
      </NextIntlClientProvider>
    ),
  });

  return { ...result, queryClient };
}
