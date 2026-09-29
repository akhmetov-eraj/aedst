import type { Metadata } from "next";
import { Footer, Layout, Navbar } from "nextra-theme-docs";
import { Head } from "nextra/components";
import { getPageMap } from "nextra/page-map";
import type { ReactNode } from "react";
import "nextra-theme-docs/style.css";

export const metadata: Metadata = {
  title: {
    default: "AEDST docs",
    template: "%s — AEDST docs",
  },
  description: "Architecture, module boundaries and decision records for the AEDST monorepo.",
};

// No GitHub remote is configured for this repo yet (see `git remote -v`) —
// update this once one exists so "Edit this page" links resolve.
const docsRepositoryBase = "https://github.com/aedst/aedst/tree/main/docs/content";

const navbar = (
  <Navbar
    logo={
      <span style={{ fontWeight: 700 }}>
        AEDST <span style={{ opacity: 0.6, fontWeight: 400 }}>docs</span>
      </span>
    }
  />
);

const footer = <Footer>MIT {new Date().getFullYear()} © AEDST.</Footer>;

export default async function RootLayout({
  children,
}: {
  children: ReactNode;
}) {
  return (
    <html lang="en" dir="ltr" suppressHydrationWarning>
      <Head />
      <body>
        <Layout
          navbar={navbar}
          pageMap={await getPageMap()}
          docsRepositoryBase={docsRepositoryBase}
          footer={footer}
          editLink="Edit this page on GitHub"
          sidebar={{ defaultMenuCollapseLevel: 1 }}
        >
          {children}
        </Layout>
      </body>
    </html>
  );
}
