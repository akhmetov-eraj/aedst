package com.aedst.core.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Fill
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstThemeExtras

/** A single labeled value plotted by [AedstBarChart] or [AedstLineChart]. */
data class AedstChartPoint(val label: String, val value: Float)

/**
 * Minimal bar chart drawn with [Canvas] only (no charting library dependency). Bars are scaled
 * to the largest value; labels are rendered below via a [Row] of [Text] sharing equal weights.
 */
@Composable
fun AedstBarChart(points: List<AedstChartPoint>, modifier: Modifier = Modifier, barColor: Color? = null) {
    val resolvedColor = barColor ?: AedstThemeExtras.extendedColors.chart2
    val axisColor = MaterialTheme.colorScheme.outlineVariant
    val maxValue = points.maxOfOrNull { it.value }?.takeIf { it > 0f } ?: 1f

    Column(modifier = modifier.fillMaxWidth()) {
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(160.dp),
        ) {
            val barCount = points.size.coerceAtLeast(1)
            val slotWidth = size.width / barCount
            val barWidth = slotWidth * 0.6f

            // Baseline axis.
            drawLine(
                color = axisColor,
                start = Offset(0f, size.height),
                end = Offset(size.width, size.height),
                strokeWidth = 2f,
            )

            points.forEachIndexed { index, point ->
                val barHeight = (point.value / maxValue) * size.height
                val left = index * slotWidth + (slotWidth - barWidth) / 2f
                drawRect(
                    color = resolvedColor,
                    topLeft = Offset(left, size.height - barHeight),
                    size = androidx.compose.ui.geometry.Size(barWidth, barHeight),
                    style = Fill,
                )
            }
        }
        Row(modifier = Modifier.fillMaxWidth().padding(top = AedstSpacing.xs)) {
            points.forEach { point ->
                Text(
                    text = point.label,
                    style = MaterialTheme.typography.labelSmall,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

/**
 * Minimal line chart drawn with [Canvas] only: a connected polyline over a lightly filled area,
 * normalized to the range of [points]' values.
 */
@Composable
fun AedstLineChart(points: List<AedstChartPoint>, modifier: Modifier = Modifier, lineColor: Color? = null) {
    val resolvedColor = lineColor ?: AedstThemeExtras.extendedColors.chart1
    val axisColor = MaterialTheme.colorScheme.outlineVariant

    Column(modifier = modifier.fillMaxWidth()) {
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(160.dp),
        ) {
            if (points.isEmpty()) return@Canvas

            val minValue = points.minOf { it.value }
            val maxValue = points.maxOf { it.value }
            val range = (maxValue - minValue).takeIf { it > 0f } ?: 1f
            val stepX = if (points.size > 1) size.width / (points.size - 1) else 0f

            fun offsetFor(index: Int, point: AedstChartPoint): Offset {
                val x = stepX * index
                val normalized = (point.value - minValue) / range
                val y = size.height - (normalized * size.height)
                return Offset(x, y)
            }

            drawLine(
                color = axisColor,
                start = Offset(0f, size.height),
                end = Offset(size.width, size.height),
                strokeWidth = 2f,
            )

            val linePoints = points.mapIndexed { index, point -> offsetFor(index, point) }

            if (linePoints.size > 1) {
                val fillPath = androidx.compose.ui.graphics.Path().apply {
                    moveTo(linePoints.first().x, size.height)
                    linePoints.forEach { lineTo(it.x, it.y) }
                    lineTo(linePoints.last().x, size.height)
                    close()
                }
                drawPath(path = fillPath, color = resolvedColor.copy(alpha = 0.15f))

                for (i in 0 until linePoints.lastIndex) {
                    drawLine(
                        color = resolvedColor,
                        start = linePoints[i],
                        end = linePoints[i + 1],
                        strokeWidth = 4f,
                        cap = androidx.compose.ui.graphics.StrokeCap.Round,
                    )
                }
            }
            linePoints.forEach { offset ->
                drawCircle(color = resolvedColor, radius = 5f, center = offset)
            }
        }
        Row(modifier = Modifier.fillMaxWidth().padding(top = AedstSpacing.xs)) {
            points.forEach { point ->
                Text(
                    text = point.label,
                    style = MaterialTheme.typography.labelSmall,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

private val samplePoints = listOf(
    AedstChartPoint("Mon", 12f),
    AedstChartPoint("Tue", 18f),
    AedstChartPoint("Wed", 9f),
    AedstChartPoint("Thu", 24f),
    AedstChartPoint("Fri", 16f),
)

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstBarChartPreview() {
    com.aedst.core.ui.theme.AedstTheme {
        Surface {
            AedstBarChart(points = samplePoints, modifier = Modifier.padding(AedstSpacing.lg))
        }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AedstLineChartPreview() {
    com.aedst.core.ui.theme.AedstTheme {
        Surface {
            AedstLineChart(points = samplePoints, modifier = Modifier.padding(AedstSpacing.lg))
        }
    }
}
