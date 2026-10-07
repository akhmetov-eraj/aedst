import type { Metadata } from "next";
import { cookies, headers } from "next/headers";
import { notFound } from "next/navigation";
import { hasLocale, NextIntlClientProvider } from "next-intl";
import { getTranslations, setRequestLocale } from "next-intl/server";
import { NuqsAdapter } from "nuqs/adapters/next/app";
import { Toaster } from "sonner";

import "@app/styles/globals.css";
import { fontArabic, fontMono, fontSans } from "@/app/config/fonts";
import {
  getLocaleDirection,
  type Locale,
  toLanguageTag,
} from "@/app/config/i18n/locales";
import { routing } from "@/app/config/i18n/routing";
import { siteConfig } from "@/app/config/seo";
import { ConsentLayer } from "@/app/providers/consent-layer";
import { PwaProvider } from "@/app/providers/pwa-provider";
import { QueryProvider } from "@/app/providers/query-provider";
import { ThemeProvider } from "@/app/providers/theme-provider";
import { OfflineBanner } from "@/app/shell/OfflineBanner";
import { AuthProvider } from "@/shared/lib/auth/auth-provider";
import { getSessionRoles, getSessionUserId } from "@/shared/lib/auth/session";
import { ConsentProvider } from "@/shared/lib/consent/consent";
import {
  CONSENT_COOKIE,
  parseConsent,
} from "@/shared/lib/consent/consent-cookie";
import { DirectionProvider } from "@/shared/ui/direction";

export async function generateMetadata({
  params,
}: LayoutProps<"/[locale]">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "common" });

  return {
    metadataBase: siteConfig.url,
    title: {
      default: t("appName"),
      template: `%s · ${t("appName")}`,
    },
    description: t("appDescription"),
    applicationName: t("appName"),
    robots: {
      index: true,
      follow: true,
      googleBot: { index: true, follow: true },
    },
    openGraph: {
      type: "website",
      siteName: t("appName"),
      title: t("appName"),
      description: t("appDescription"),
      locale: toLanguageTag(locale as Locale),
      url: `/${locale}`,
    },
    twitter: {
      card: "summary_large_image",
      title: t("appName"),
      description: t("appDescription"),
    },
  };
}

export function generateStaticParams() {
  return routing.locales.map((locale) => ({ locale }));
}

export default async function RootLayout({
  children,
  params,
}: LayoutProps<"/[locale]">) {
  const { locale } = await params;

  if (!hasLocale(routing.locales, locale)) {
    notFound();
  }

  setRequestLocale(locale);

  const direction = getLocaleDirection(locale as Locale);
  const t = await getTranslations({ locale, namespace: "common" });
  // Reading request data is what opts the tree into dynamic rendering, which
  // the per-request CSP nonce (set by proxy.ts) requires. The nonce is
  // handed to next-themes' inline no-flash script so the strict CSP allows it.
  const nonce = (await headers()).get("x-nonce") ?? undefined;
  const userId = (await getSessionUserId()) ?? null;
  const roles = await getSessionRoles();
  const consent = parseConsent((await cookies()).get(CONSENT_COOKIE)?.value);

  return (
    <html
      lang={toLanguageTag(locale as Locale)}
      dir={direction}
      className={`${fontSans.variable} ${fontArabic.variable} ${fontMono.variable} h-full antialiased`}
      suppressHydrationWarning
    >
      <body className="flex min-h-full flex-col">
        <a
          href="#main-content"
          className="sr-only focus:not-sr-only focus:fixed focus:start-4 focus:top-4 focus:z-50 focus:rounded-md focus:bg-background focus:px-4 focus:py-2 focus:font-medium focus:text-foreground focus:shadow-lg focus:ring-2 focus:ring-ring"
        >
          {t("a11y.skipToContent")}
        </a>

        <NextIntlClientProvider>
          <DirectionProvider direction={direction}>
            <ThemeProvider
              attribute="class"
              defaultTheme="system"
              enableSystem
              disableTransitionOnChange
              nonce={nonce}
            >
              <PwaProvider>
                <OfflineBanner />
                <NuqsAdapter>
                  <AuthProvider initialUserId={userId} initialRoles={roles}>
                    <QueryProvider>
                      <main
                        id="main-content"
                        tabIndex={-1}
                        className="flex flex-1 flex-col outline-none"
                      >
                        {children}
                      </main>
                    </QueryProvider>
                  </AuthProvider>
                </NuqsAdapter>
              </PwaProvider>

              <Toaster richColors dir={direction} />
              <ConsentProvider initial={consent}>
                <ConsentLayer />
              </ConsentProvider>
            </ThemeProvider>
          </DirectionProvider>
        </NextIntlClientProvider>
      </body>
    </html>
  );
}
