import { env } from "@/app/config/env";

/**
 * Better Uptime has no SDK — monitoring is just a periodic GET to a
 * heartbeat URL from your Better Uptime dashboard. Call this from a cron
 * route / scheduled job once NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL is set;
 * it's a no-op without it so this stays safe to call unconditionally.
 */
export async function sendBetterUptimeHeartbeat() {
  if (!env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL) {
    return;
  }

  await fetch(env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL, { method: "GET" });
}
