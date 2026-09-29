package com.aedst.core.analytics

import android.content.Context
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.crashlytics.FirebaseCrashlytics
import com.google.firebase.perf.FirebasePerformance
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object AnalyticsModule {

    @Provides
    @Singleton
    fun provideAnalyticsTracker(@ApplicationContext context: Context): AnalyticsTracker =
        if (BuildConfig.ANALYTICS_ENABLED) {
            FirebaseAnalyticsTracker(
                FirebaseAnalytics.getInstance(context),
                FirebaseCrashlytics.getInstance(),
                FirebasePerformance.getInstance(),
            )
        } else {
            NoOpAnalyticsTracker()
        }
}
