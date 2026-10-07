# ADR-0027: Real-time notifications and native toasts

- Status: accepted (not compiled in the authoring environment)
- Scope: `apps/desktop` (`Features/Notifications`, `Aedst::Platform`, `Aedst::Contract`)
- Supersedes in part: ADR-0006 (Windows toasts were "not implemented"), ADR-0022 (the claim "no push service on desktop"; the tray stays as fallback and as the icon)
- Needs from the backend: [proposal `WatchNotifications`](../proposals/notifications-watch-rpc.md) (plan rule 5: the protos are not changed from the desktop side)

## Context

Desktop polled `GetUnreadCount` every 30 s and showed tray balloons. That gives up to 30 s of latency, constant
idle traffic and no actionable notifications. The server has no streaming RPC for the inbox, so the first
deliverable is the client side plus the proposal for the backend.

## Decisions

1. **Server stream, polling as the safety net.** `NotificationStream` (Domain port) over a future server-streaming
   `WatchNotifications`. `NotificationRealtime` owns the connection:
   - reconnect with exponential backoff and equal jitter (1 s -> 60 s); the counter resets only after a stream stayed up
     for 10 s, so a server that accepts and drops cannot be hammered;
   - resume with the last event id; `ResyncRequired` or a rejected resume point clears it and makes the owner refetch;
   - idle watchdog: 90 s without any event (heartbeats count) reconnects a half-open connection;
   - after 3 consecutive failures, or when the RPC does not exist (`unimplemented`/404), the mode becomes *Polling*; the
     stream is tried again after 5 minutes (1 hour for "unsupported");
   - an `unauthenticated` end stops the supervisor and is reported to the `ErrorSink` (sign-out path).
   `NotificationsViewModel` polls whenever the stream is not healthy and stops polling while it is; a mode change to
   non-streaming polls immediately, so there is no blind spot while reconnecting.
2. **Contract gating.** The streaming table (`streaming_methods.hpp`), the adapter (`ConnectNotificationStream`) and its wiring
   exist only when `notifications.proto` declares `rpc WatchNotifications` (CMake detects it and defines
   `AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS`). No server change is needed to merge this work, and deployment order does
   not matter. The adapter assumes the message names of the proposal; other names change one file.
3. **Native notifications with a click.** `NotificationService::showRequest()` carries a link, a replace tag and an
   action label; `setActivationHandler()` reports clicks.
   - **Windows:** `WindowsToastNotificationService` (C++/WinRT, headers from the Windows SDK, no new package). It sets an
     AppUserModelID (`com.aedst.Desktop`), makes sure a Start Menu shortcut with that AUMID exists (the installer should create it; the
     service creates it otherwise), builds ToastGeneric XML (`buildToastXml`, escaped, links limited to relative routes) with an "Open"
     button, and maps `ToastNotification.Activated` to the GUI thread. If Windows refuses toasts (user setting, policy,
     COM failure) the tray balloon is used.
   - **Linux:** `org.freedesktop.Notifications` over QtDBus now sends a `default` action, tracks the server id of each
     notification, replaces by tag (`replaces_id`) and handles `ActionInvoked`/`NotificationClosed`.
   - **Tray:** `QSystemTrayIcon` stays as the fallback and the tray icon with the unread tooltip; a balloon click reports the
     link of the last request.
   - A click raises the window and opens the link through `Router::openLink` (allow-list), for example `/tasks/<id>`; the
     "N more" summary links to `/notifications`.
4. **Docs.** "No push service on desktop" is removed; the accurate statement is "live stream when the server offers it,
   polling otherwise".

## Limits and UNVERIFIED

- Toast activation works while the app runs. A click after the app exited only starts it through the shortcut and loses the link;
  fixing that needs the COM activator (`INotificationActivationCallback` + `ToastActivatorCLSID` on the shortcut) or the
  `aedst://` protocol registration from the installer (Phase 8).
- The C++/WinRT code, the shortcut creation and the D-Bus action handling have never been compiled or run.
  The XML builder and the supervisor are covered by tests that do not need Windows or a bus.
- Wayland/GNOME servers differ in how they render the `default` action (some show a button, some only make the body clickable).
- `QSystemTrayIcon` still forces Qt Widgets (`QApplication`); removing it is possible once both native paths are proven.

## Consequences

- Latency drops from up to 30 s to about a second once the backend ships the RPC; idle traffic drops to heartbeats.
- Until then behaviour is unchanged except that notifications are clickable on Windows and Linux.
- `NotificationService` gained a default-implemented `showRequest`/`setActivationHandler`; existing services keep working.

## Revisit when

The backend ships the RPC (switch on, remove the gating if all deployments have it), the installer provides the COM activator,
or `QSystemTrayIcon` can be dropped.
