import { Code, ConnectError } from "@connectrpc/connect";

/**
 * The app's one error vocabulary. Every failure — a ConnectError from
 * apps/server, a dropped network, a bug — is classified into one of these
 * kinds, and everything downstream (retry policy, toast text, Sentry
 * reporting, error boundaries) decides by kind, never by parsing messages.
 * Server messages are diagnostic English and never shown to users; each
 * kind has a translated message under `common.errors.kinds`.
 */
export type AppErrorKind =
  | "unauthenticated"
  | "forbidden"
  | "notFound"
  | "invalidInput"
  | "conflict"
  | "rateLimited"
  | "unavailable"
  | "timeout"
  | "offline"
  | "cancelled"
  | "unknown";

export type AppError = {
  kind: AppErrorKind;
  /** The Connect code name when the error came from an RPC. */
  code?: string;
  /** Developer-facing detail; never render it. */
  message: string;
  cause: unknown;
};

const KIND_BY_CODE: Partial<Record<Code, AppErrorKind>> = {
  [Code.Unauthenticated]: "unauthenticated",
  [Code.PermissionDenied]: "forbidden",
  [Code.NotFound]: "notFound",
  [Code.InvalidArgument]: "invalidInput",
  [Code.OutOfRange]: "invalidInput",
  [Code.FailedPrecondition]: "conflict",
  [Code.AlreadyExists]: "conflict",
  [Code.Aborted]: "conflict",
  [Code.ResourceExhausted]: "rateLimited",
  [Code.Unavailable]: "unavailable",
  [Code.DeadlineExceeded]: "timeout",
  [Code.Canceled]: "cancelled",
};

function isOffline() {
  return typeof navigator !== "undefined" && navigator.onLine === false;
}

export function toAppError(error: unknown): AppError {
  if (error instanceof ConnectError) {
    let kind = KIND_BY_CODE[error.code] ?? "unknown";
    if ((kind === "unavailable" || kind === "unknown") && isOffline()) {
      kind = "offline";
    }
    return {
      kind,
      code: Code[error.code],
      message: error.rawMessage,
      cause: error,
    };
  }
  if (error instanceof DOMException && error.name === "AbortError") {
    return { kind: "cancelled", message: error.message, cause: error };
  }
  if (error instanceof DOMException && error.name === "TimeoutError") {
    return { kind: "timeout", message: error.message, cause: error };
  }
  if (isOffline()) {
    return { kind: "offline", message: String(error), cause: error };
  }
  return {
    kind: "unknown",
    message: error instanceof Error ? error.message : String(error),
    cause: error,
  };
}

/**
 * Definitive answers from the server (the "4xx" family): asking again gives
 * the same answer, so they are never retried and never reported to Sentry —
 * they are control flow, not incidents.
 */
const DEFINITIVE: ReadonlySet<AppErrorKind> = new Set([
  "unauthenticated",
  "forbidden",
  "notFound",
  "invalidInput",
  "conflict",
  "cancelled",
]);

export function isRetryable(error: AppError): boolean {
  return !DEFINITIVE.has(error.kind);
}

/**
 * Worth an incident: server faults and bugs. Expected states (offline, a
 * rate limit, a client-side cancel, definitive answers) are not.
 */
export function shouldReport(error: AppError): boolean {
  return error.kind === "unknown" || error.kind === "unavailable";
}

/** The `common.errors.kinds.*` message key for a kind. */
export function errorMessageKey(kind: AppErrorKind) {
  return `kinds.${kind}` as const;
}

/**
 * Exponential backoff with full jitter for retryable failures; a rate
 * limit waits longer. Capped so a user is never left waiting for minutes.
 */
export function retryDelayMs(attempt: number, error: AppError): number {
  const base = error.kind === "rateLimited" ? 2_000 : 500;
  const ceiling = Math.min(base * 2 ** attempt, 15_000);
  return Math.round(ceiling / 2 + Math.random() * (ceiling / 2));
}
