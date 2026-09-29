package com.aedst.core.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider

private val LightColors = lightColorScheme(
    background = AedstLightBackground,
    onBackground = AedstLightForeground,
    surface = AedstLightCard,
    onSurface = AedstLightCardForeground,
    surfaceVariant = AedstLightMuted,
    onSurfaceVariant = AedstLightMutedForeground,
    surfaceTint = AedstLightPrimary,
    inverseSurface = AedstDarkCard,
    inverseOnSurface = AedstDarkCardForeground,
    primary = AedstLightPrimary,
    onPrimary = AedstLightPrimaryForeground,
    primaryContainer = AedstLightAccent,
    onPrimaryContainer = AedstLightAccentForeground,
    secondary = AedstLightSecondary,
    onSecondary = AedstLightSecondaryForeground,
    tertiary = AedstLightAccent,
    onTertiary = AedstLightAccentForeground,
    error = AedstLightDestructive,
    onError = AedstLightDestructiveForeground,
    outline = AedstLightInput,
    outlineVariant = AedstLightBorder,
)

private val DarkColors = darkColorScheme(
    background = AedstDarkBackground,
    onBackground = AedstDarkForeground,
    surface = AedstDarkCard,
    onSurface = AedstDarkCardForeground,
    surfaceVariant = AedstDarkMuted,
    onSurfaceVariant = AedstDarkMutedForeground,
    surfaceTint = AedstDarkPrimary,
    inverseSurface = AedstLightCard,
    inverseOnSurface = AedstLightCardForeground,
    primary = AedstDarkPrimary,
    onPrimary = AedstDarkPrimaryForeground,
    primaryContainer = AedstDarkAccent,
    onPrimaryContainer = AedstDarkAccentForeground,
    secondary = AedstDarkSecondary,
    onSecondary = AedstDarkSecondaryForeground,
    tertiary = AedstDarkAccent,
    onTertiary = AedstDarkAccentForeground,
    error = AedstDarkDestructive,
    onError = AedstDarkDestructiveForeground,
    outline = AedstDarkInput,
    outlineVariant = AedstDarkBorder,
)

/**
 * Root theme for every `core.ui` component. Wraps [MaterialTheme] with the AEDST semantic color
 * scheme, typography and shapes, and exposes [LocalAedstExtendedColors] for chart/sidebar tokens
 * that Material3's [androidx.compose.material3.ColorScheme] has no slot for.
 */
@Composable
fun AedstTheme(
    useDarkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    val colorScheme = if (useDarkTheme) DarkColors else LightColors
    val extendedColors = if (useDarkTheme) AedstDarkExtendedColors else AedstLightExtendedColors

    CompositionLocalProvider(LocalAedstExtendedColors provides extendedColors) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = AedstTypography,
            shapes = AedstShapes,
            content = content,
        )
    }
}

/** Convenience accessor: `AedstTheme.extendedColors` from inside any `@Composable`. */
object AedstThemeExtras {
    val extendedColors: AedstExtendedColors
        @Composable get() = LocalAedstExtendedColors.current
}
