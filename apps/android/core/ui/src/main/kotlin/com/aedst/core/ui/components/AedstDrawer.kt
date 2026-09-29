package com.aedst.core.ui.components

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material3.DrawerState
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.Icon
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.NavigationDrawerItemDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.aedst.core.ui.theme.AedstSpacing
import com.aedst.core.ui.theme.AedstTheme
import com.aedst.core.ui.theme.AedstThemeExtras

/**
 * Navigation drawer primitive, mirroring web's `sidebar.tsx` in its overlay/mobile mode. Wraps
 * Material3's [ModalNavigationDrawer] + [ModalDrawerSheet], themed with the sidebar token family.
 */
@Composable
fun AedstDrawer(
    drawerContent: @Composable ColumnScope.() -> Unit,
    modifier: Modifier = Modifier,
    drawerState: DrawerState = rememberDrawerState(DrawerValue.Closed),
    gesturesEnabled: Boolean = true,
    content: @Composable () -> Unit,
) {
    val extendedColors = AedstThemeExtras.extendedColors
    ModalNavigationDrawer(
        modifier = modifier,
        drawerState = drawerState,
        gesturesEnabled = gesturesEnabled,
        drawerContent = {
            ModalDrawerSheet(
                drawerContainerColor = extendedColors.sidebar,
                drawerContentColor = extendedColors.sidebarForeground,
                content = drawerContent,
            )
        },
        content = content,
    )
}

/**
 * A single selectable row inside [AedstDrawer]'s `drawerContent`, mirroring web's
 * `sidebar-menu-button.tsx`.
 */
@Composable
fun AedstDrawerItem(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    icon: (@Composable () -> Unit)? = null,
) {
    val extendedColors = AedstThemeExtras.extendedColors
    NavigationDrawerItem(
        label = { Text(label) },
        selected = selected,
        onClick = onClick,
        modifier = modifier,
        icon = icon,
        colors = NavigationDrawerItemDefaults.colors(
            selectedContainerColor = extendedColors.sidebarAccent,
            selectedTextColor = extendedColors.sidebarAccentForeground,
            selectedIconColor = extendedColors.sidebarAccentForeground,
            unselectedTextColor = extendedColors.sidebarForeground,
            unselectedIconColor = extendedColors.sidebarForeground,
        ),
    )
}

@Preview(name = "Light", showBackground = true)
@Preview(name = "Dark", showBackground = true, uiMode = android.content.res.Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun AedstDrawerPreview() {
    AedstTheme {
        val extendedColors = AedstThemeExtras.extendedColors
        Surface(color = extendedColors.sidebar) {
            Column(modifier = Modifier.padding(AedstSpacing.md)) {
                AedstDrawerItem(
                    label = "Inbox",
                    selected = true,
                    onClick = {},
                    icon = { Icon(Icons.Default.Check, contentDescription = null) },
                )
                AedstDrawerItem(
                    label = "Archive",
                    selected = false,
                    onClick = {},
                )
            }
        }
    }
}
