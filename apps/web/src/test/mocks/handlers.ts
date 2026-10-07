import type { AnyHandler } from "msw";

/**
 * Default handlers: none. Every test declares the Connect procedures it
 * depends on with `rpcHandler`/`rpcErrorHandler` (see ./connect.ts), and
 * `onUnhandledFrame: "error"` in vitest.setup.ts turns any call a test did
 * not expect into a failure instead of a silent network attempt.
 */
export const handlers: AnyHandler[] = [];
