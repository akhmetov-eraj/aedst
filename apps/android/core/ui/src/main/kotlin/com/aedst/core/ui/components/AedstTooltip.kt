package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.PlainTooltip
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TooltipBox
import androidx.compose.material3.TooltipDefaults
import androidx.compose.material3.rememberTooltipState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Wraps Material3's [TooltipBox] with a single-label [PlainTooltip], shown above [content] on
 * long-press (touch) or hover (pointer). Mirrors web's `tooltip.tsx`.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AedstTooltip(
    tooltipText: String,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    TooltipBox(
        positionProvider = TooltipDefaults.rememberTooltipPositionProvider(),
        tooltip = { PlainTooltip { Text(tooltipText) } },
        state = rememberTooltipState(),
        modifier = modifier,
        content = content,
    )
}

@Preview(name = "Light")
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstTooltipPreview() {
    AedstTheme {
        Surface {
            Box(modifier = Modifier.padding(AedstSpacing.xxxl)) {
                AedstTooltip(tooltipText = "More information") {
                    IconButton(onClick = {}) {
                        Icon(imageVector = Icons.Default.Info, contentDescription = "Info")
                    }
                }
            }
        }
    }
}
