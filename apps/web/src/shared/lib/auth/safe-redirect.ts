const FALLBACK = "/tasks";

/**
 * Validates a user-supplied post-login destination (the `next` query
 * param). Only same-origin absolute paths pass: anything else — protocol
 * URLs, `//host`, backslash tricks, control characters — falls back, so the
 * login flow can never be turned into an open redirect.
 */
export function safeRedirectPath(
  candidate: string | null | undefined,
  fallback: string = FALLBACK,
): string {
  if (!candidate?.startsWith("/")) {
    return fallback;
  }
  if (candidate.startsWith("//") || candidate.includes("\\")) {
    return fallback;
  }
  // biome-ignore lint/suspicious/noControlCharactersInRegex: rejecting control chars is the point
  if (/[\u0000-\u001f\u007f]/.test(candidate)) {
    return fallback;
  }
  return candidate;
}
