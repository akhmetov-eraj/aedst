# AedstDesignSystem

SwiftUI design system for `apps/apple`, built to feature/quality parity with
`apps/web/src/shared/ui/` (shadcn/ui). All tokens and components live under
`Sources/AedstDesignSystem/`.

## Tokens

Design tokens are **generated** from `packages/design-tokens/tokens` (single source for all platforms; see that README). Files marked GENERATED must not be edited — change the JSON and run `pnpm --dir packages/design-tokens build`.

| File | Contents |
|---|---|
| `AedstLightPalette.swift`, `AedstDarkPalette.swift` | GENERATED. Raw light/dark `Color` values per semantic token (web-parity names). |
| `AedstColor.swift` | Hand-written pairing of the two palettes into adaptive light/dark `Color`s (`AedstColor.background`, `.primary`, ..., `chartSeries`). Legacy names (`brandPrimary`, `surface`, `textPrimary`, `textSecondary`, `danger`) stay as `@available(*, deprecated)` aliases. |
| `AedstSpacing.swift` | GENERATED. `CGFloat` scale `none`, `xxs`..`giant` (4pt grid; note `md` is 12, not the pre-token 16). |
| `AedstRadius.swift` | GENERATED. `CGFloat` scale `xs`..`xxxl`, `full`. |
| `AedstElevation.swift` | GENERATED. `CGFloat` elevation levels (reference values). |
| `AedstMotion.swift` | GENERATED. Animation durations in seconds. |
| `AedstTypeScale.swift` | GENERATED. Cross-platform type scale (reference values). Apple text deliberately uses Dynamic Type via `AedstTypography` instead. |
| `AedstTypography.swift` | Semantic `Font` scale on Dynamic-Type text styles: `largeTitle`, `title`, `headline`, `body`, `caption`. |

## Components (`Components/`)

**Layout** — `AedstCard`, `AedstSeparator`, `AedstAspectRatioModifier` (`.aedstAspectRatio()`), `AedstScrollArea`, `AedstCollapsible`

**Actions** — `AedstButton` (rewritten: variants `primary/secondary/destructive/outline/ghost/link`, sizes `sm/md/lg/icon`, loading + disabled states), `AedstIconButton`, `AedstToggleButton`, `AedstToggleGroup`

**Forms** — `AedstTextField` (rewritten: placeholder, helper/error text, disabled), `AedstTextEditor`, `AedstLabel`, `AedstCheckbox`, `AedstSwitch`, `AedstRadioGroup`, `AedstSelect`, `AedstSlider`, `AedstOTPInput`, `AedstCombobox`

**Feedback** — `AedstAlert`, `AedstAlertDialogModifier` (`.aedstAlertDialog()`), `AedstToastModifier` (`.aedstToast()`), `AedstProgressView`, `AedstSkeleton`, `AedstBadge`, `AedstTooltipModifier` (`.aedstTooltip()`)

**Overlay/Nav** — `AedstDialogModifier` (`.aedstDialog()`), `AedstSheetModifier` (`.aedstSheet()`), `AedstDrawerModifier` (`.aedstDrawer()`), `AedstPopoverModifier` (`.aedstPopover()`), `AedstDropdownMenu`, `AedstContextMenuModifier` (`.aedstContextMenu()`), `AedstMenuBar`, `AedstTabBar`, `AedstCommandPalette`, `AedstHoverCardModifier` (`.aedstHoverCard()`)

**Data display** — `AedstAvatar`, `AedstTable`, `AedstDatePicker`, `AedstCarousel`, `AedstAccordion`, `AedstTabs`, `AedstBreadcrumb`, `AedstPagination`, `AedstChart` (Swift Charts)

Every component has a `#Preview` (light and, for the two most-used, dark) and
accessibility labels/hints/values on interactive elements. Typography always
goes through `AedstTypography`/`.font()` on top of Dynamic-Type-aware
`Font.system(...)` text styles — no hardcoded point sizes.

## Deliberate adaptations (web concept → SwiftUI/HIG equivalent)

- **Sheet** → native `.sheet()` wrapper (`AedstSheetModifier`), not a custom overlay.
- **Drawer** → custom side-anchored panel (`AedstDrawerModifier`), since `.sheet()` is bottom-only on iOS; drawer suits macOS/iPad auxiliary panels.
- **Dialog** vs **AlertDialog** → kept as two separate types: `AedstDialogModifier` is a custom centered card (web's `Dialog`), `AedstAlertDialogModifier` wraps the native `.alert()` (web's `AlertDialog`/confirm flows) — they read differently on web too.
- **NavigationMenu** → split into `AedstTabBar` (native bottom `TabView`, top-level app navigation) and `AedstTabs` (inline underline-indicator switcher, for in-page tab content) rather than one literal port.
- **Command** (⌘K palette) → `AedstCommandPalette`, a `.searchable` `List` in a sheet — the idiomatic SwiftUI equivalent; there's no floating-overlay command palette convention on Apple platforms.
- **ContextMenu** → thin `.contextMenu()` wrapper (`AedstContextMenuModifier`) reusing the same `AedstMenuAction` model as `AedstDropdownMenu`.
- **HoverCard** → `AedstHoverCardModifier` uses real `.onHover` on macOS; other platforms (no pointer) fall back to a long-press-triggered popover, since there's no touch equivalent of "hover."
- **Menubar** → `AedstMenuBar` only makes sense on macOS (there is no menu bar elsewhere); it degrades to an inline `Menu` row on other platforms so the same call site compiles everywhere.
- **Checkbox** / **RadioGroup** → hand-built (`RoundedRectangle`+checkmark / `Circle`+dot), since neither has a native SwiftUI control on iOS.
- **Skeleton** → hand-built shimmer (`LinearGradient` sweep over a muted block), since SwiftUI has no native placeholder-loading view.
- **Toast** → hand-built overlay + auto-dismiss `ViewModifier`, since SwiftUI has no native toast/snackbar.
- **Table** → row-based `VStack` (works everywhere, including narrow iPhone widths); a literal grid with native sort/resize would need macOS's `Table` view specifically, which isn't universal across the package's platform targets, so it was kept to one cross-platform implementation.
- **Chart** → thin wrapper around first-party **Swift Charts** (`AedstChart`, bar/line/area) rather than a hand-rolled renderer, per the task's own guidance.

Nothing from the requested catalog was skipped outright — every item maps to
exactly one file above, adapted where a literal 1:1 port didn't fit SwiftUI/HIG.

## Verification

This is a Windows sandbox with no Swift toolchain (`which swift` → not found), so
`swift build` could not be run. Instead, every file was manually re-read for:

- Signature/call-site consistency (checked existing consumers in
  `Features/Auth/Sources/Auth/Presentation/LoginView.swift` and
  `App/Shared/Sources/AedstAppShared/RootView.swift` against the rewritten
  `AedstButton`/`AedstTextField` — both only use the subset of parameters that
  kept the same names and defaults, so they still compile).
- Platform-availability guards: fixed three issues found this way —
  `SearchFieldPlacement.navigationBarDrawer` (iOS-only) removed from
  `AedstCommandPalette`, `.indexViewStyle` (unavailable on macOS) guarded with
  `#if !os(macOS)` in `AedstCarousel`, and a `Chart` closure in `AedstChart`
  changed from an (index, point) tuple-shuffle parameter list (removed from
  Swift years ago) to a single destructured parameter.
- `Package.swift` platform minimums are now iOS 15 / macOS 12 / watchOS 8 / tvOS 15 /
  visionOS 1; APIs above that (two-parameter `.onChange`, `presentationCompactAdaptation`,
  `presentationDetents`, `scrollContentBackground`, `Font.system(_:design:weight:)`, `Duration`,
  Swift Charts, `NavigationStack`) are replaced or `#available`-gated — see `apps/apple/README.md`.

No `swift build` / `xcodebuild` was actually executed — this repo has no macOS
CI runner available in this session either, so a real compiler pass is still
recommended before merging.
