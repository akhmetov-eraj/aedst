/**
 * Registry of feature modules that ship their own translation catalogs
 * (`src/modules/<module>/i18n/<locale>.json`). Adding a module here is
 * the only wiring needed to get it merged into the request-scoped
 * `messages` object under its own namespace — see `messages.ts`.
 *
 * Kept as an explicit array (not a filesystem scan) so the dynamic
 * `import(\`@/modules/${module}/i18n/${locale}.json\`)` in messages.ts
 * stays statically analyzable by the bundler.
 */
export const i18nModules = ["home"] as const;

export type I18nModule = (typeof i18nModules)[number];
