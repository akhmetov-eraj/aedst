"use client";

import { TransportProvider } from "@connectrpc/connect-query";
import { QueryClientProvider } from "@tanstack/react-query";
import { ReactQueryDevtools } from "@tanstack/react-query-devtools";
import { useState } from "react";
import { transport } from "@/shared/api/connect/client";
import { makeQueryClient } from "@/shared/lib/query-client";

export function QueryProvider({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  const [queryClient] = useState(() => makeQueryClient());

  return (
    <TransportProvider transport={transport}>
      <QueryClientProvider client={queryClient}>
        {children}

        {process.env.NODE_ENV === "development" && (
          <ReactQueryDevtools
            initialIsOpen={false}
            buttonPosition="bottom-right"
            position="bottom"
            theme="system"
          />
        )}
      </QueryClientProvider>
    </TransportProvider>
  );
}
