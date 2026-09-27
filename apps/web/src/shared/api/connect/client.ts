import { createClient } from "@connectrpc/connect";
import { createConnectTransport } from "@connectrpc/connect-web";

import { env } from "@/app/config/env";
import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { ProfileService } from "@/shared/api/connect/generated/aedst/profile/v1/profile_pb";
import { UsersService } from "@/shared/api/connect/generated/aedst/users/v1/users_pb";

/**
 * The Connect-ES transport shared by every generated client. ConnectRPC's
 * wire protocol is plain HTTP/1.1 + JSON (see docs/decisions/0002-connectrpc.md),
 * so the browser can call apps/server directly — no gRPC-Web proxy needed.
 */
export const transport = createConnectTransport({
  baseUrl: env.NEXT_PUBLIC_API_URL,
});

export const authClient = createClient(AuthService, transport);
export const usersClient = createClient(UsersService, transport);
export const profileClient = createClient(ProfileService, transport);

// Re-exported so callers using `@connectrpc/connect-query`'s `useQuery` /
// `useMutation` (which take a method descriptor, e.g. `AuthService.method.login`)
// don't need their own import path into `generated/`.
export { AuthService, ProfileService, UsersService };
