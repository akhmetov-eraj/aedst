package com.aedst.core.ui.components

import android.content.res.Configuration
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstShapesScale
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme

/**
 * Filterable combobox over an arbitrary option type [T], mirroring web's `command.tsx` combobox
 * pattern. The anchor field is editable; typing narrows [options] to those whose [optionLabel]
 * contains the query (case-insensitive), and picking one syncs the field text back to the
 * selection's label.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun <T> AedstSearchableDropdown(
    options: List<T>,
    selected: T?,
    onSelectedChange: (T) -> Unit,
    modifier: Modifier = Modifier,
    label: String? = null,
    optionLabel: (T) -> String,
    enabled: Boolean = true,
) {
    var expanded by remember { mutableStateOf(false) }
    var query by remember(selected) { mutableStateOf(selected?.let(optionLabel) ?: "") }

    val filteredOptions = remember(options, query) {
        if (query.isEmpty()) {
            options
        } else {
            options.filter { optionLabel(it).contains(query, ignoreCase = true) }
        }
    }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { if (enabled) expanded = it },
        modifier = modifier.fillMaxWidth().semantics { if (!enabled) disabled() },
    ) {
        OutlinedTextField(
            value = query,
            onValueChange = {
                query = it
                expanded = true
            },
            modifier = Modifier.fillMaxWidth().menuAnchor(ExposedDropdownMenuAnchorType.PrimaryEditable, enabled),
            enabled = enabled,
            label = label?.let { { Text(it) } },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            shape = AedstShapesScale.medium,
        )
        if (filteredOptions.isNotEmpty()) {
            ExposedDropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false },
            ) {
                filteredOptions.forEach { option ->
                    DropdownMenuItem(
                        text = { Text(optionLabel(option)) },
                        onClick = {
                            onSelectedChange(option)
                            query = optionLabel(option)
                            expanded = false
                        },
                    )
                }
            }
        }
    }
}

@Preview(name = "Light")
@Preview(name = "Dark", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstSearchableDropdownPreview() {
    AedstTheme {
        Surface(color = MaterialTheme.colorScheme.background) {
            AedstSearchableDropdown(
                options = listOf("Armenia", "Argentina", "Australia", "Austria"),
                selected = "Argentina",
                onSelectedChange = {},
                modifier = Modifier.padding(AedstSpacing.lg),
                label = "Country",
                optionLabel = { it },
            )
        }
    }
}
