const STORAGE_KEY = "aedst.device-id";

let memory: string | undefined;

/**
 * A stable per-browser id `AuthService.Login`/`Register` use for session
 * tracking (see apps/server's `auth.sessions.device_id`). Persisted so the
 * same browser reuses one server-side session slot across visits; falls back
 * to a per-page-load id where storage is unavailable (private mode, blocked
 * site data) — the id is an identifier, not a credential, so that is safe.
 */
export function getDeviceId(): string {
  if (memory) {
    return memory;
  }
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    if (stored) {
      memory = stored;
      return stored;
    }
  } catch {
    // Storage unavailable — fall through to an in-memory id.
  }

  memory = `web-${crypto.randomUUID()}`;
  try {
    window.localStorage.setItem(STORAGE_KEY, memory);
  } catch {
    // Keep the in-memory id.
  }
  return memory;
}
