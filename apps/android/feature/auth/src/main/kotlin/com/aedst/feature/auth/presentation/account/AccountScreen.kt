package com.aedst.feature.auth.presentation.account

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.aedst.core.ui.components.AedstButton
import com.aedst.core.ui.components.AedstButtonVariant
import com.aedst.core.ui.components.AedstSwitch
import com.aedst.feature.auth.R

@Composable
fun AccountRoute(viewModel: AccountViewModel = hiltViewModel()) {
    val userId by viewModel.userId.collectAsStateWithLifecycle()
    val isSigningOut by viewModel.isSigningOut.collectAsStateWithLifecycle()
    val isAnalyticsEnabled by viewModel.isAnalyticsEnabled.collectAsStateWithLifecycle()

    AccountScreen(
        userId = userId,
        isSigningOut = isSigningOut,
        isAnalyticsEnabled = isAnalyticsEnabled,
        onAnalyticsEnabledChange = viewModel::setAnalyticsEnabled,
        onSignOut = viewModel::signOut,
    )
}

@Composable
internal fun AccountScreen(
    userId: String?,
    isSigningOut: Boolean,
    onSignOut: () -> Unit,
    isAnalyticsEnabled: Boolean = false,
    onAnalyticsEnabledChange: (Boolean) -> Unit = {},
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .safeDrawingPadding()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        if (userId != null) {
            Text(
                text = stringResource(R.string.auth_signed_in_as, userId),
                style = MaterialTheme.typography.bodyLarge,
            )
        }
        AedstSwitch(
            checked = isAnalyticsEnabled,
            onCheckedChange = onAnalyticsEnabledChange,
            label = stringResource(R.string.auth_analytics_consent),
            modifier = Modifier.fillMaxWidth(),
        )
        AedstButton(
            text = stringResource(R.string.auth_sign_out),
            onClick = onSignOut,
            variant = AedstButtonVariant.OUTLINE,
            loading = isSigningOut,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}
