package com.aedst.core.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/** Radius scale mirroring web's `--radius` token (`0.625rem` base) and its derived steps. */
object AedstRadius {
    val sm: Dp = 6.dp
    val md: Dp = 8.dp
    val lg: Dp = 10.dp
    val xl: Dp = 14.dp
    val xxl: Dp = 18.dp
    val xxxl: Dp = 22.dp
    val full: Dp = 999.dp
}

object AedstShapesScale {
    val small = RoundedCornerShape(AedstRadius.sm)
    val medium = RoundedCornerShape(AedstRadius.md)
    val large = RoundedCornerShape(AedstRadius.lg)
    val extraLarge = RoundedCornerShape(AedstRadius.xl)
    val pill = RoundedCornerShape(AedstRadius.full)
}
