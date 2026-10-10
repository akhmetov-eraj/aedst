import QtQuick
import QtTest
import AedstDesignSystem

// Responsive and text-scale contract of Theme: breakpoints are ordered, the OS text size can only grow the UI (never
// below 1x, capped at 2x), and every scaled font follows textScale. Runs headless (QT_QPA_PLATFORM=offscreen).
TestCase {
    name: "Theme"

    function test_breakpointsAreStrictlyIncreasing() {
        verify(Theme.breakpointSm > 0)
        verify(Theme.breakpointSm < Theme.breakpointMd)
        verify(Theme.breakpointMd < Theme.breakpointLg)
    }

    function test_textScaleStaysWithinOneAndTwo() {
        verify(Theme.textScale >= 1)
        verify(Theme.textScale <= 2)
    }

    function test_fontsFollowTheTextScale() {
        verify(Theme.bodyFont.pixelSize > 0)
        verify(Theme.titleFont.pixelSize >= Theme.headingFont.pixelSize)
        verify(Theme.headingFont.pixelSize >= Theme.bodyFont.pixelSize)
        verify(Theme.bodyFont.pixelSize >= Theme.smallFont.pixelSize)
    }

    function test_spacingScaleIsMonotonic() {
        verify(Theme.spacingXxs < Theme.spacingXs)
        verify(Theme.spacingXs < Theme.spacingSm)
        verify(Theme.spacingSm < Theme.spacingMd)
        verify(Theme.spacingMd < Theme.spacingLg)
        verify(Theme.spacingLg < Theme.spacingXl)
    }

    // A view asks its own window width (see the comment in Theme.qml): the three classes partition every width.
    function test_widthClassesPartitionTheRange() {
        const classify = function (width) {
            return width < Theme.breakpointMd ? "compact" : (width < Theme.breakpointLg ? "regular" : "wide")
        }
        compare(classify(Theme.breakpointSm), "compact")
        compare(classify(Theme.breakpointMd - 1), "compact")
        compare(classify(Theme.breakpointMd), "regular")
        compare(classify(Theme.breakpointLg - 1), "regular")
        compare(classify(Theme.breakpointLg), "wide")
    }
}
