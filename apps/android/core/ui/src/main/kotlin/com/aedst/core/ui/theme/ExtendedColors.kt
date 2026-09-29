package com.aedst.core.ui.theme

import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color

/**
 * Tokens that Material3's [androidx.compose.material3.ColorScheme] has no slot for, but web's
 * token set defines: chart series colors and the sidebar surface family.
 */
@Immutable
data class AedstExtendedColors(
    val chart1: Color,
    val chart2: Color,
    val chart3: Color,
    val chart4: Color,
    val chart5: Color,
    val sidebar: Color,
    val sidebarForeground: Color,
    val sidebarPrimary: Color,
    val sidebarPrimaryForeground: Color,
    val sidebarAccent: Color,
    val sidebarAccentForeground: Color,
    val sidebarBorder: Color,
    val sidebarRing: Color,
) {
    val chartColors: List<Color> get() = listOf(chart1, chart2, chart3, chart4, chart5)
}

val AedstLightExtendedColors = AedstExtendedColors(
    chart1 = AedstLightChart1,
    chart2 = AedstLightChart2,
    chart3 = AedstLightChart3,
    chart4 = AedstLightChart4,
    chart5 = AedstLightChart5,
    sidebar = AedstLightSidebar,
    sidebarForeground = AedstLightSidebarForeground,
    sidebarPrimary = AedstLightSidebarPrimary,
    sidebarPrimaryForeground = AedstLightSidebarPrimaryForeground,
    sidebarAccent = AedstLightSidebarAccent,
    sidebarAccentForeground = AedstLightSidebarAccentForeground,
    sidebarBorder = AedstLightSidebarBorder,
    sidebarRing = AedstLightSidebarRing,
)

val AedstDarkExtendedColors = AedstExtendedColors(
    chart1 = AedstDarkChart1,
    chart2 = AedstDarkChart2,
    chart3 = AedstDarkChart3,
    chart4 = AedstDarkChart4,
    chart5 = AedstDarkChart5,
    sidebar = AedstDarkSidebar,
    sidebarForeground = AedstDarkSidebarForeground,
    sidebarPrimary = AedstDarkSidebarPrimary,
    sidebarPrimaryForeground = AedstDarkSidebarPrimaryForeground,
    sidebarAccent = AedstDarkSidebarAccent,
    sidebarAccentForeground = AedstDarkSidebarAccentForeground,
    sidebarBorder = AedstDarkSidebarBorder,
    sidebarRing = AedstDarkSidebarRing,
)

val LocalAedstExtendedColors = staticCompositionLocalOf { AedstLightExtendedColors }
