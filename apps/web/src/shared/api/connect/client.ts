import { createClient } from "@connectrpc/connect";
import { createConnectTransport } from "@connectrpc/connect-web";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { TasksService } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { clientEnv } from "@/shared/env/client";
import { authInterceptor } from "@/shared/lib/auth/auth-interceptor";

/**
 * The Connect-ES transport shared by every generated client. ConnectRPC's
 * wire protocol is plain HTTP/1.1 + JSON (see docs/content/decisions/0002-connectrpc.md),
 * so the browser can call apps/server directly — no gRPC-Web proxy needed.
 * `authInterceptor` attaches `Authorization: Bearer <accessToken>` from
 * client-side token storage (see shared/lib/auth) to every call; the four
 * public AuthService procedures ignore it since the server's own
 * AuthInterceptor never checks it for them.
 */
export const transport = createConnectTransport({
  baseUrl: clientEnv.NEXT_PUBLIC_API_BASE_URL,
  interceptors: [authInterceptor],
});

export const authClient = createClient(AuthService, transport);
export const tasksClient = createClient(TasksService, transport);

// Re-exported so callers using `@connectrpc/connect-query`'s `useQuery` /
// `useMutation` (which take a method descriptor, e.g. `AuthService.method.login`)
// don't need their own import path into `generated/`.
export { AuthService, TasksService };
