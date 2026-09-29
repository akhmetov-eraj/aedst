/**
 * One id per browser session (module-level, not per component mount) — a
 * stable device identity `AuthService.Login`/`Register` use for session
 * tracking (see apps/server's `auth.sessions.device_id`).
 */
let cached: string | undefined;

export function deviceId(): string {
  cached ??= `web-${Math.random().toString(36).slice(2)}`;
  return cached;
}
