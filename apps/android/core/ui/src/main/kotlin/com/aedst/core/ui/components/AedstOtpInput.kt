package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.aedst.core.ui.R
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Fixed-[length] one-time-passcode entry, mirroring web's `input-otp.tsx`. A single invisible
 * [BasicTextField] captures keyboard input while a row of bordered cells renders the digits;
 * the cell matching the current cursor position is highlighted with the primary color to show
 * focus. Input is filtered to digits and clamped to [length].
 */
@Composable
fun AedstOtpInput(
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    length: Int = 6,
    enabled: Boolean = true,
) {
    var isFocused by remember { mutableStateOf(false) }
    val otpDescription = stringResource(R.string.aedst_a11y_otp_progress, value.length, length)

    Box(
        modifier = modifier.semantics {
            contentDescription = otpDescription
            if (!enabled) disabled()
        },
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(AedstSpacing.sm)) {
            for (index in 0 until length) {
                val char = value.getOrNull(index)?.toString() ?: ""
                val isActiveCell =
                    isFocused && (index == value.length || (index == length - 1 && value.length == length))
                OtpCell(char = char, isActive = isActiveCell, enabled = enabled)
            }
        }

        BasicTextField(
            value = value,
            onValueChange = { input ->
                val digitsOnly = input.filter { it.isDigit() }.take(length)
                onValueChange(digitsOnly)
            },
            modifier = Modifier
                .size(width = (length * 48).dp, height = 48.dp)
                .onFocusChanged { isFocused = it.isFocused },
            enabled = enabled,
            singleLine = true,
            textStyle = TextStyle(color = Color.Transparent),
            cursorBrush = androidx.compose.ui.graphics.SolidColor(Color.Transparent),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
        )
    }
}

@Composable
private fun OtpCell(char: String, isActive: Boolean, enabled: Boolean) {
    val borderColor = when {
        isActive -> MaterialTheme.colorScheme.primary
        else -> MaterialTheme.colorScheme.outline
    }
    Box(
        modifier = Modifier
            .size(40.dp)
            .border(width = if (isActive) 2.dp else 1.dp, color = borderColor, shape = AedstShapesScale.small)
            .background(
                color = if (enabled) MaterialTheme.colorScheme.surface else MaterialTheme.colorScheme.surfaceVariant,
                shape = AedstShapesScale.small,
            ),
        contentAlignment = Alignment.Center,
    ) {
        Text(text = char, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
    }
}

@Preview(name = "Light")
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstOtpInputPreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            Box(modifier = Modifier.padding(AedstSpacing.lg)) {
                AedstOtpInput(value = "12", onValueChange = {}, length = 6)
            }
        }
    }
}
