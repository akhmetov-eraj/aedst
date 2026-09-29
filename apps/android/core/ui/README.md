# core:ui — Aedst Design System (Jetpack Compose)

Compose equivalent of the web app's shadcn/ui component library (`apps/web/src/shared/ui/`), built to the same semantic-token parity. See [CONVENTIONS.md](../../../../../CONVENTIONS.md) for the naming rules every file here follows.

## Tokens (`theme/`)

| File | Purpose |
|---|---|
| `theme/Color.kt` | Full light+dark semantic palette, ported from web's `globals.css` OKLCH values (background/foreground, card, popover, primary/secondary/muted/accent + foregrounds, destructive, border/input/ring, chart-1..5, sidebar-*). |
| `theme/Spacing.kt` | `AedstSpacing` scale (`none`..`giant`). |
| `theme/Radius.kt` | `AedstRadius` scale + `AedstShapesScale` (small/medium/large/extraLarge/pill). |
| `theme/Elevation.kt` | `AedstElevation` scale (`none`..`level5`). |
| `theme/ExtendedColors.kt` | `AedstExtendedColors` + `LocalAedstExtendedColors` CompositionLocal for tokens with no Material3 `ColorScheme` slot (chart series, sidebar family). Access via `AedstThemeExtras.extendedColors` inside any `@Composable`. |
| `theme/Theme.kt` | `AedstTheme` — builds `lightColorScheme`/`darkColorScheme` from the palette and provides both the Material3 scheme and `LocalAedstExtendedColors`. |
| `theme/Type.kt` | `AedstTypography` (display/headline/title/body/label scales). |
| `theme/Shape.kt` | Built from `AedstShapesScale`. |

## Components (`components/`)

44 components, one `PascalCase.kt` file per public composable's main type, each with light+dark `@Preview` pairs wrapped in `AedstTheme { Surface { ... } }`.

| Category | Components |
|---|---|
| Layout | `AedstCard` (+Header/Title/Description/Content/Footer slots), `AedstDivider`, `AspectRatioBox`, `AedstScrollArea` (+Horizontal), `AedstExpandableSection` |
| Actions | `AedstButton` (rewritten: variant/size enums, loading+disabled), `AedstIconButton`, `AedstToggleButton`, `AedstToggleGroup` |
| Forms | `AedstTextField` (+`AedstPasswordTextField`), `AedstTextArea`, `AedstLabel`, `AedstCheckbox`, `AedstSwitch`, `AedstRadioGroup`, `AedstSelect`, `AedstSlider` (+`AedstRangeSlider`), `AedstOtpInput`, `AedstSearchableDropdown` |
| Feedback | `AedstAlert`, `AedstAlertDialog`, `AedstSnackbar`, `AedstProgressIndicator` (linear+circular), `AedstSkeleton` (+Text), `AedstBadge`, `AedstTooltip` |
| Overlay/Nav | `AedstDialog`, `AedstBottomSheet`, `AedstDrawer` (+Item), `AedstPopover`, `AedstDropdownMenu`, `AedstContextMenu`, `AedstTopAppBar`, `AedstBottomNavigationBar`, `AedstCommandPalette`, `AedstLongPressPreview` |
| Data display | `AedstAvatar`, `AedstDataTable`, `AedstDatePicker`, `AedstCarousel`, `AedstAccordion`, `AedstTabs`, `AedstBreadcrumb`, `AedstPagination`, `AedstChart` (Bar+Line, Canvas-drawn) |

Plus pre-existing `LoadingIndicator.kt` (`FullScreenLoading`), left untouched.

## Deliberate adaptations vs. web's shadcn catalog

- **Sheet** → `AedstBottomSheet` (`ModalBottomSheet`); **Drawer** stays a separate side-drawer (`ModalNavigationDrawer`) since Android has genuine use for both.
- **NavigationMenu** → split into `AedstTopAppBar` + `AedstBottomNavigationBar` (no single web-equivalent nav primitive maps to Android's top/bottom bar split).
- **Command** → `AedstCommandPalette` (searchable `LazyColumn`, grouped).
- **HoverCard** → `AedstLongPressPreview` (no hover on touch; long-press + `AedstPopover` is the idiomatic substitute).
- **Menubar** → intentionally **skipped**. There's no desktop-menubar paradigm on Android; `AedstDropdownMenu`/`AedstContextMenu` cover the same interaction need.
- **Combobox** → `AedstSearchableDropdown` (filterable) alongside plain `AedstSelect` (non-filterable).
- Avatar image loading: no image-loading dependency (Coil) exists in this module yet, so `AedstAvatar`'s `imageUrl` param is accepted but not yet wired — it always renders the initials fallback. Wire it up when Coil (or an equivalent) is added.

## Verification status

No JDK/Gradle is available in the environment these components were built in, so **no real `./gradlew :core:ui:compileDebugKotlin` has run yet**. Every file went through a manual self-review pass (import correctness, brace/paren balance, cross-file symbol references) by the agent that wrote it, plus a repo-wide consistency pass afterward that confirmed:
- No duplicate top-level declaration names across all 44 component files.
- Every `AedstSpacing.*` / `AedstRadius.*` / `AedstElevation.*` / `AedstShapesScale.*` / `AedstThemeExtras.extendedColors.*` token referenced by a component actually exists in `theme/`.
- All 44 component files are brace-balanced.

**Before merging:** run a real Gradle build (`./gradlew :core:ui:compileDebugKotlin`), then `ktlint`/`detekt` (see root `CONVENTIONS.md` → Внедрение) — this is a necessary follow-up, not optional.
