package com.aedst.android

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.aedst.core.common.SessionState
import com.aedst.core.navigation.AedstRoutes
import com.aedst.core.network.NetworkMonitor
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import javax.inject.Inject

/**
 * App-shell state: which graph to show (from the session), whether to show the offline banner and the admin entry,
 * and a pending deep link from a tapped notification.
 */
@HiltViewModel
class MainViewModel @Inject constructor(sessionState: SessionState, networkMonitor: NetworkMonitor) : ViewModel() {

    /** `null` while the persisted session is being restored — the splash screen stays up until this is non-null. */
    val isSignedIn: StateFlow<Boolean?> = sessionState.isSignedIn

    val isAdmin: StateFlow<Boolean> = sessionState.currentUser
        .map { it?.isAdmin == true }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), false)

    val isOffline: StateFlow<Boolean> = networkMonitor.isOnline
        .map { online -> !online }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(STOP_TIMEOUT_MS), false)

    private val mutablePendingRoute = MutableStateFlow<String?>(null)

    /** Route to open once signed in (from a notification's `link`); consumed by the nav host. */
    val pendingRoute: StateFlow<String?> = mutablePendingRoute.asStateFlow()

    /** Accepts only links [AedstRoutes.routeForLink] validates; anything else is ignored. */
    fun onDeepLink(link: String?) {
        AedstRoutes.routeForLink(link)?.let { mutablePendingRoute.value = it }
    }

    fun onDeepLinkHandled() {
        mutablePendingRoute.value = null
    }

    private companion object {
        const val STOP_TIMEOUT_MS = 5_000L
    }
}
