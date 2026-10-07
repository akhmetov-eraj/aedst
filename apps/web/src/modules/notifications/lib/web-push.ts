/**
 * Web Push on the web, with the platform's standard Push API only:
 * `PushManager.subscribe` with the server's VAPID key, then
 * `RegisterPushDevice(WEB_PUSH)`. This one path covers Chromium, Firefox,
 * desktop Safari and iOS/iPadOS 16.4+ home-screen apps (Apple delivers
 * standard Web Push through APNs itself) — so no Firebase SDK: FCM and
 * APNs device tokens are the *native* apps' channels.
 */

/** Where this browser's registered push device id is remembered (not secret). */
const DEVICE_KEY = "aedst-push-device-id";

export type PushSupport =
  | "supported"
  /** No service worker / PushManager / Notification API. */
  | "unsupported"
  /** iOS/iPadOS Safari tab: Web Push needs the app installed to the home screen. */
  | "requires-install";

export function getPushSupport(): PushSupport {
  if (typeof window === "undefined") {
    return "unsupported";
  }
  const hasApis =
    "serviceWorker" in navigator &&
    "PushManager" in window &&
    "Notification" in window;
  if (hasApis) {
    return "supported";
  }
  const isIos =
    /iPad|iPhone|iPod/.test(navigator.userAgent) ||
    (navigator.userAgent.includes("Macintosh") && navigator.maxTouchPoints > 1);
  return isIos ? "requires-install" : "unsupported";
}

/** VAPID base64url public key → the bytes `applicationServerKey` expects. */
export function base64UrlToBytes(value: string): Uint8Array<ArrayBuffer> {
  const base64 = value.replace(/-/g, "+").replace(/_/g, "/");
  const padded = base64.padEnd(Math.ceil(base64.length / 4) * 4, "=");
  const binary = atob(padded);
  const bytes = new Uint8Array(new ArrayBuffer(binary.length));
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

export type WebPushKeys = { endpoint: string; p256dh: string; auth: string };

export function subscriptionKeys(subscription: PushSubscription): WebPushKeys {
  const json = subscription.toJSON();
  const p256dh = json.keys?.p256dh;
  const auth = json.keys?.auth;
  if (!json.endpoint || !p256dh || !auth) {
    throw new Error("push subscription is missing its keys");
  }
  return { endpoint: json.endpoint, p256dh, auth };
}

export async function getActiveSubscription(): Promise<PushSubscription | null> {
  if (getPushSupport() !== "supported") {
    return null;
  }
  const registration = await navigator.serviceWorker.getRegistration();
  return (await registration?.pushManager.getSubscription()) ?? null;
}

/**
 * Asks for permission (must run inside a user gesture) and subscribes.
 * Re-uses an existing subscription for the same key.
 */
export async function subscribeToPush(
  vapidPublicKey: string,
): Promise<PushSubscription> {
  const permission = await Notification.requestPermission();
  if (permission !== "granted") {
    throw new PushPermissionError(permission);
  }
  const registration = await navigator.serviceWorker.ready;
  const existing = await registration.pushManager.getSubscription();
  if (existing) {
    return existing;
  }
  return await registration.pushManager.subscribe({
    userVisibleOnly: true,
    applicationServerKey: base64UrlToBytes(vapidPublicKey),
  });
}

export class PushPermissionError extends Error {
  constructor(readonly permission: NotificationPermission) {
    super(`notification permission ${permission}`);
    this.name = "PushPermissionError";
  }
}

export function rememberDeviceId(deviceId: string | null) {
  try {
    if (deviceId) {
      localStorage.setItem(DEVICE_KEY, deviceId);
    } else {
      localStorage.removeItem(DEVICE_KEY);
    }
  } catch {
    // Storage disabled: the device is unregistered server-side on expiry.
  }
}

export function rememberedDeviceId(): string | null {
  try {
    return localStorage.getItem(DEVICE_KEY);
  } catch {
    return null;
  }
}

/** "Chrome · Windows" — shown in the user's device list, no fingerprinting detail. */
export function describeBrowser(userAgent: string): string {
  const browser = /Edg\//.test(userAgent)
    ? "Edge"
    : /Firefox\//.test(userAgent)
      ? "Firefox"
      : /Chrome\//.test(userAgent)
        ? "Chrome"
        : /Safari\//.test(userAgent)
          ? "Safari"
          : "Browser";
  const os = /Windows/.test(userAgent)
    ? "Windows"
    : /Android/.test(userAgent)
      ? "Android"
      : /iPhone|iPad|iPod/.test(userAgent)
        ? "iOS"
        : /Mac OS X/.test(userAgent)
          ? "macOS"
          : /Linux/.test(userAgent)
            ? "Linux"
            : "";
  return os ? `${browser} · ${os}` : browser;
}
