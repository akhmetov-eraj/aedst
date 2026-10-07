"use server";

import { cookies } from "next/headers";
import { isSecureSite } from "@/shared/lib/auth/session-cookies";
import {
  CONSENT_COOKIE,
  CONSENT_MAX_AGE_SECONDS,
} from "@/shared/lib/consent/consent-cookie";

/** Records the visitor's analytics choice (`granted` | `denied`) for a year. */
export async function saveConsentAction(choice: unknown): Promise<void> {
  if (choice !== "granted" && choice !== "denied") {
    return;
  }
  (await cookies()).set(CONSENT_COOKIE, choice, {
    httpOnly: false,
    secure: isSecureSite,
    sameSite: "lax",
    path: "/",
    maxAge: CONSENT_MAX_AGE_SECONDS,
  });
}
