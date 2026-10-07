import { createClient } from "@connectrpc/connect";
import { getTranslations } from "next-intl/server";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { getServerTransport } from "@/shared/api/connect/server";

/**
 * Server Component: `AuthService.GetCurrentUser` runs on the server with the
 * session cookie's bearer token, so the signed-in email is in the streamed
 * HTML — no client round trip and no blank while loading. Identity is
 * derived server-side from the token, never from a client-supplied id (see
 * tasks.proto's service doc comment for why that pattern matters).
 */
export async function CurrentUser() {
  const t = await getTranslations("auth");

  try {
    const { user } = await createClient(
      AuthService,
      getServerTransport(),
    ).getCurrentUser({});
    if (!user) {
      return null;
    }
    return (
      <p className="text-muted-foreground text-sm">
        {t("signedInAs", { email: user.email })}
      </p>
    );
  } catch {
    // A failure here must not take the whole page down; the data-bearing
    // sections below surface auth problems themselves.
    return null;
  }
}
