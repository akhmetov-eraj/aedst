# AedstDesignSystem

SwiftUI design system for `apps/apple`, built to feature/quality parity with
`apps/web/src/shared/ui/` (shadcn/ui). All tokens and components live under
`Sources/AedstDesignSystem/`.

## Tokens

| File | Contents |
|---|---|
| `AedstColor.swift` | Semantic adaptive light/dark colors mirroring web's CSS custom properties in `apps/web/src/app/styles/globals.css`: `background`/`foreground`, `card`/`cardForeground`, `popover`/`popoverForeground`, `primary`/`primaryForeground`, `secondary`/`secondaryForeground`, `muted`/`mutedForeground`, `accent`/`accentForeground`, `destructive`/`destructiveForeground`, `success`/`successForeground`, `warning`/`warningForeground`, `border`, `input`, `ring`, `chart1`...`chart5` (+ `chartSeries`). Legacy names (`brandPrimary`, `surface`, `textPrimary`, `textSecondary`, `danger`) are kept as `@available(*, deprecated)` aliases so existing call sites (`Features/Auth`, `App/Shared`) keep compiling. |
| `AedstSpacing.swift` | 4pt-grid scale: `xxs`...`xxxl`. |
| `AedstRadius.swift` | Corner radius scale mirroring web's `--radius-*` derived properties: `xs`...`xxl`, `full`. |
| `AedstTypography.swift` | Semantic `Font` scale: `largeTitle`, `title`, `headline`, `body`, `caption`. |

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
- `Package.swift` platform minimums (iOS 17 / macOS 14 / watchOS 10 / tvOS 17 /
  visionOS 1) against every API used (two-parameter `.onChange`, `.popover`
  `presentationCompactAdaptation`, `@Previewable`, Swift Charts) — all satisfied.

No `swift build` / `xcodebuild` was actually executed — this repo has no macOS
CI runner available in this session either, so a real compiler pass is still
recommended before merging.
