package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstTheme

/**
 * Thin wrapper around [Modifier.aspectRatio], mirroring web's `AspectRatio` primitive so callers
 * reach for one shared name instead of applying the modifier ad hoc at each call site.
 */
@Composable
fun AspectRatioBox(ratio: Float, modifier: Modifier = Modifier, content: @Composable BoxScope.() -> Unit) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .aspectRatio(ratio),
        content = content,
    )
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES, showBackground = true)
@Composable
private fun AspectRatioBoxPreview() {
    AedstTheme {
        Surface {
            AspectRatioBox(
                ratio = 16f / 9f,
                modifier = Modifier
                    .background(MaterialTheme.colorScheme.surfaceVariant, AedstShapesScale.medium),
            ) {
                Text(
                    text = "16:9",
                    modifier = Modifier.align(Alignment.Center),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
