package com.aedst.android.tv

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import androidx.tv.material3.Surface
import com.aedst.android.tv.navigation.AppTvNavHost
import com.aedst.core.ui.theme.AedstTheme
import dagger.hilt.android.AndroidEntryPoint

/**
 * TV counterpart to `:app`'s `MainActivity`. Same minimal-entry-point shape
 * (theme -> full-screen surface -> nav host), wiring the shared
 * `:feature:auth` nav graph via [AppTvNavHost] instead of duplicating any of
 * its business logic. Uses `androidx.tv.material3.Surface` rather than
 * Material3's `Surface` so the root container gets TV-appropriate focus/
 * color-group handling out of the box.
 */
@AndroidEntryPoint
class MainTvActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AedstTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    AppTvNavHost()
                }
            }
        }
    }
}
