package com.aedst.core.ui.components

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstTheme

/** A single destination in [AedstBottomNavigationBar]. */
data class AedstBottomNavItem(
    val label: String,
    val icon: @Composable () -> Unit,
    val selectedIcon: (@Composable () -> Unit)? = null,
)

/**
 * Bottom navigation bar, mirroring web's tab-bar navigation pattern. Wraps Material3's
 * [NavigationBar] / [NavigationBarItem].
 */
@Composable
fun AedstBottomNavigationBar(
    items: List<AedstBottomNavItem>,
    selectedIndex: Int,
    onItemSelected: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    NavigationBar(
        modifier = modifier,
        containerColor = MaterialTheme.colorScheme.surface,
        contentColor = MaterialTheme.colorScheme.onSurface,
    ) {
        items.forEachIndexed { index, item ->
            val isSelected = index == selectedIndex
            NavigationBarItem(
                selected = isSelected,
                onClick = { onItemSelected(index) },
                icon = {
                    if (isSelected && item.selectedIcon != null) {
                        item.selectedIcon.invoke()
                    } else {
                        item.icon()
                    }
                },
                label = { Text(item.label) },
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = MaterialTheme.colorScheme.onSecondary,
                    selectedTextColor = MaterialTheme.colorScheme.onSurface,
                    indicatorColor = MaterialTheme.colorScheme.secondary,
                    unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                    unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                ),
            )
        }
    }
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", showBackground = true, uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstBottomNavigationBarPreview() {
    AedstTheme {
        var selectedIndex by remember { mutableIntStateOf(0) }
        AedstBottomNavigationBar(
            items = listOf(
                AedstBottomNavItem(
                    label = "Tasks",
                    icon = { Icon(Icons.Default.Check, contentDescription = null) },
                ),
                AedstBottomNavItem(
                    label = "Search",
                    icon = { Icon(Icons.Default.Search, contentDescription = null) },
                ),
            ),
            selectedIndex = selectedIndex,
            onItemSelected = { selectedIndex = it },
        )
    }
}
