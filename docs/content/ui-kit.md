# UI Kit — cross-platform component parity

AEDST ships one design system with four renderings: **Web** (shadcn/ui,
`apps/web/src/shared/ui/`), **Android** (Jetpack Compose,
`apps/android/core/ui/`), **Apple** (SwiftUI,
`apps/apple/Core/AedstDesignSystem/`), and **Desktop** (Qt Quick/QML,
`apps/desktop/Modules/Core/DesignSystem/`). Web is the source of truth;
each native platform's README documents its own adaptations in full detail —
this page is the cross-platform index, not a duplicate of any of them.

A separate TV-specific surface exists on Android only:
`apps/android/core/ui-tv/` (`androidx.tv:tv-material`-based), used solely by
the `:app-tv` module — see [architecture-android.md](/architecture-android#phone-and-tv-a-second-thin-app-target)
for why that's a Gradle module boundary, not a fork of this table's Android
column.

## Tokens

| Token category | Web | Android | Apple | Desktop |
|---|---|---|---|---|
| Color (OKLCH source) | `src/app/styles/globals.css` | `core/ui/theme/Color.kt` | `AedstColor.swift` | `ThemeController`/`Theme.qml` (hand-converted to sRGB hex) |
| Spacing | Tailwind scale | `core/ui/theme/Spacing.kt` (`AedstSpacing`) | `AedstSpacing.swift` (4pt grid) | `Theme.qml` (`spacingXs`..`spacing2xl`) |
| Radius | `--radius-*` | `core/ui/theme/Radius.kt` (`AedstRadius`) | `AedstRadius.swift` | `Theme.qml` (`radiusSm`..`radius2xl`) |
| Typography | Tailwind type scale | `core/ui/theme/Type.kt` (`AedstTypography`) | `AedstTypography.swift` | `Theme.qml` font roles |
| Elevation | shadow utilities | `core/ui/theme/Elevation.kt` (`AedstElevation`) | native SwiftUI shadow modifiers | QML `MultiEffect`/native elevation |

All four derive from the same web `globals.css` palette; Desktop's mapping
is the one manual step (no OKLCH parser in Qt — see its README for the
conversion table) and is the first place to check if a color looks off after
a web palette change.

## Component parity

| Component (web name) | Web | Android | Apple | Desktop |
|---|---|---|---|---|
| Accordion | `accordion.tsx` | `AedstAccordion` | `AedstAccordion` | `AedstAccordion.qml` (stack of `AedstCollapsible`) |
| Alert | `alert.tsx` | `AedstAlert` | `AedstAlert` | `AedstAlert.qml` |
| AlertDialog | `alert-dialog.tsx` | `AedstAlertDialog` | `AedstAlertDialogModifier` (native `.alert()`) | `AedstAlertDialog.qml` |
| Avatar | `avatar.tsx` | `AedstAvatar` (image loading not wired — no Coil dep yet) | `AedstAvatar` | `AedstAvatar.qml` (circular mask via `MultiEffect`) |
| Badge | `badge.tsx` | `AedstBadge` | `AedstBadge` | `AedstBadge.qml` |
| Breadcrumb | `breadcrumb.tsx` | `AedstBreadcrumb` | `AedstBreadcrumb` | `AedstBreadcrumb.qml` |
| Button | `button.tsx` | `AedstButton` | `AedstButton` | `AedstButton.qml` |
| Calendar | `calendar.tsx` | `AedstDatePicker` | `AedstDatePicker` | `AedstCalendar.qml` (hand-rolled month grid; QQC2's `Calendar` was removed) |
| Card | `card.tsx` | `AedstCard` (+slots) | `AedstCard` | `AedstCard.qml` |
| Carousel | `carousel.tsx` | `AedstCarousel` | `AedstCarousel` | `AedstCarousel.qml` (`T.SwipeView`-based) |
| Chart | `chart.tsx` (Recharts) | `AedstChart` (Canvas-drawn bar+line) | `AedstChart` (Swift Charts) | `AedstChart.qml` (Canvas-drawn, single-series only) |
| Checkbox | `checkbox.tsx` | `AedstCheckbox` | `AedstCheckbox` (hand-built, no native iOS control) | `AedstCheckBox.qml` (tri-state) |
| Collapsible | `collapsible.tsx` | `AedstExpandableSection` | `AedstCollapsible` | `AedstCollapsible.qml` |
| Combobox | `combobox.tsx` | `AedstSearchableDropdown` | `AedstCombobox` | `AedstSearchableComboBox.qml` |
| Command | `command.tsx` | `AedstCommandPalette` | `AedstCommandPalette` (`.searchable` List in a sheet) | `AedstSearchableList.qml` |
| ContextMenu | `context-menu.tsx` | `AedstContextMenu` | `AedstContextMenuModifier` | `AedstContextMenu.qml` (native right-click `Menu`) |
| Dialog | `dialog.tsx` | `AedstDialog` | `AedstDialogModifier` (custom centered card) | `AedstDialog.qml` |
| Drawer | `drawer.tsx` | `AedstDrawer` (side, `ModalNavigationDrawer`) | `AedstDrawerModifier` (custom side panel) | `AedstDrawer.qml` |
| DropdownMenu | `dropdown-menu.tsx` | `AedstDropdownMenu` | `AedstDropdownMenu` | `AedstDropdownMenu.qml` |
| HoverCard | `hover-card.tsx` | `AedstLongPressPreview` (no hover on touch) | `AedstHoverCardModifier` (real hover on macOS, long-press elsewhere) | `AedstHoverCard.qml` (ToolTip-based, real hover) |
| InputOTP | `input-otp.tsx` | `AedstOtpInput` | `AedstOTPInput` | `AedstOtpInput.qml` |
| Label | `label.tsx` | `AedstLabel` | `AedstLabel` | `AedstLabel.qml` |
| Menubar | `menubar.tsx` | **skipped** (no menubar paradigm on Android) | `AedstMenuBar` (macOS only; inline `Menu` elsewhere) | `AedstMenuBar.qml` |
| NavigationMenu | `navigation-menu.tsx` | split: `AedstTopAppBar` + `AedstBottomNavigationBar` | split: `AedstTabBar` (bottom nav) + `AedstTabs` (in-page) | `AedstTabBar.qml` |
| Pagination | `pagination.tsx` | `AedstPagination` | `AedstPagination` | `AedstPagination.qml` |
| Popover | `popover.tsx` | `AedstPopover` | `AedstPopoverModifier` | `AedstPopover.qml` |
| Progress | `progress.tsx` | `AedstProgressIndicator` (linear+circular) | `AedstProgressView` | `AedstProgressBar.qml` / `AedstCircularProgress.qml` |
| RadioGroup | `radio-group.tsx` | `AedstRadioGroup` | `AedstRadioGroup` (hand-built) | `AedstRadioButton.qml` / `AedstRadioGroup.qml` |
| ScrollArea | `scroll-area.tsx` | `AedstScrollArea` (+Horizontal) | `AedstScrollArea` | `AedstScrollArea.qml` |
| Select | `select.tsx` | `AedstSelect` | `AedstSelect` | `AedstComboBox.qml` |
| Separator | `separator.tsx` | `AedstDivider` | `AedstSeparator` | `AedstSeparator.qml` |
| Sheet | `sheet.tsx` | `AedstBottomSheet` (`ModalBottomSheet`) | native `.sheet()` (`AedstSheetModifier`) | mapped to `AedstDrawer.qml` |
| Skeleton | `skeleton.tsx` | `AedstSkeleton` (+Text) | `AedstSkeleton` (hand-built shimmer) | `AedstSkeleton.qml` |
| Slider | `slider.tsx` | `AedstSlider` (+Range) | `AedstSlider` | `AedstSlider.qml` |
| Switch | `switch.tsx` | `AedstSwitch` | `AedstSwitch` | `AedstSwitch.qml` |
| Table | `table.tsx` | `AedstDataTable` | `AedstTable` (row-based `VStack`, cross-platform) | `AedstTableView.qml` |
| Tabs | `tabs.tsx` | `AedstTabs` | `AedstTabs` | covered by `AedstTabBar.qml` |
| Textarea | `textarea.tsx` | `AedstTextArea` | `AedstTextEditor` | `AedstTextArea.qml` |
| Toast | (Sonner) | `AedstSnackbar` | hand-built `AedstToastModifier` | `AedstToast.qml` (single instance, no global queue yet) |
| Toggle / ToggleGroup | `toggle(-group).tsx` | `AedstToggleButton` / `AedstToggleGroup` | `AedstToggleButton` / `AedstToggleGroup` | `AedstToggleButton.qml` / `AedstToggleGroup.qml` |
| Tooltip | `tooltip.tsx` | `AedstTooltip` | `AedstTooltipModifier` | `AedstToolTip.qml` |
| Input / TextField | `input.tsx` | `AedstTextField` (+`AedstPasswordTextField`) | `AedstTextField` (rewritten) | `AedstTextField.qml` (rewritten) |
| IconButton (no direct web file — `button.tsx` `size="icon"`) | — | `AedstIconButton` | `AedstIconButton` | `AedstIconButton.qml` |
| AspectRatio | `aspect-ratio.tsx` | `AspectRatioBox` | `AedstAspectRatioModifier` | `AedstAspectRatio.qml` |

Not every web primitive has a native equivalent and vice versa (e.g. web's
`sidebar.tsx`/`resizable.tsx`/`command`-adjacent pieces map onto the rows
above or are covered by platform-native navigation chrome instead of a 1:1
port) — when in doubt, the three platform READMEs linked below are the
authoritative, fully-detailed source; this table exists so a change to one
platform's component surface can be checked for parity across the other
three at a glance.

## Full detail per platform

- [`apps/android/core/ui/README.md`](../../apps/android/core/ui/README.md) — Compose components, adaptation rationale, verification status.
- [`apps/android/core/ui-tv/README.md`](../../apps/android/core/ui-tv/README.md) — TV-only Compose surface (`:core:ui-tv`).
- [`apps/apple/Core/AedstDesignSystem/README.md`](../../apps/apple/Core/AedstDesignSystem/README.md) — SwiftUI components, HIG adaptation rationale.
- [`apps/desktop/Modules/Core/DesignSystem/README.md`](../../apps/desktop/Modules/Core/DesignSystem/README.md) — QML components, Qt Quick Controls adaptation rationale.

All three native READMEs currently carry the same caveat: no Gradle/Xcode/Qt
toolchain was available when they were written, so components are
structurally reviewed but not yet compiler-verified — run the platform's
real build before treating any of them as final.
