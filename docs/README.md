# aedst-docs

The AEDST documentation site: a [Nextra 4](https://nextra.site) docs-theme app (Next.js App Router, TypeScript) that publishes `content/` — architecture, module boundaries, and ADRs for the whole monorepo.

## Tech stack

- **Next.js 16** (App Router, Turbopack) + **React 19**
- **Nextra 4** (`nextra` + `nextra-theme-docs`), TypeScript throughout (`.tsx`, not `.jsx`)
- **Pagefind** for full-text search, indexed as a `postbuild` step

## Getting started

```bash
pnpm install
pnpm dev
```

Open [http://localhost:3000](http://localhost:3000).

## Adding a page

Add a `.md`/`.mdx` file under `content/` (nested folders map to nested routes) and list it in that folder's `_meta.ts` for a sidebar title and explicit ordering — an unlisted page still renders, appended alphabetically. Link to other pages by absolute site path (e.g. `[modules](/modules)`), not by relative `.md` filename — Nextra does not guarantee `.md`-suffixed relative links resolve to the right route.

## Build

```bash
pnpm build   # next build, then pagefind indexing (postbuild)
pnpm start   # serve the production build
```

`pnpm build` alone does not produce a working search index — the `postbuild` script (`pagefind --site .next/server/app --output-path public/_pagefind`) must run after it; `pnpm build` runs both.

## Known upstream issue: pinned `zod`

`pnpm-workspace.yaml` pins `zod` to `4.3.6` via `overrides`. `nextra-theme-docs@4.6.1`'s `<Layout>` throws `Invalid input: expected nonoptional, received undefined -> at children` on `zod >=4.4.0` (upstream: [shuding/nextra#4989](https://github.com/shuding/nextra/issues/4989), [#5008](https://github.com/shuding/nextra/issues/5008), [#5036](https://github.com/shuding/nextra/issues/5036)). Drop the override once a `nextra-theme-docs` release ships the fix from [#4990](https://github.com/shuding/nextra/pull/4990).
