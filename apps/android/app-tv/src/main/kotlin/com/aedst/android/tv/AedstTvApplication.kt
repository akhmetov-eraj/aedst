package com.aedst.android.tv

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

/**
 * TV counterpart to `:app`'s `AedstApplication`. Not shared from `:app`
 * directly: `AedstApplication` lives in `:app`'s own module (`com.aedst.android`),
 * not a `:core:*` module, so `:app-tv` cannot depend on it without depending
 * on the whole phone application module. This class stays a minimal
 * `@HiltAndroidApp` entry point — identical DI bootstrap pattern, own
 * package — with all real business logic living in the shared `:core:*` /
 * `:feature:*` modules both apps depend on.
 */
@HiltAndroidApp
class AedstTvApplication : Application()
