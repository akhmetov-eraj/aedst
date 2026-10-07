package com.aedst.core.sync

import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import dagger.multibindings.Multibinds

@Module
@InstallIn(SingletonComponent::class)
abstract class SyncModule {
    /** Declares the (possibly empty) set that features add [OutboxHandler]s to with `@IntoSet`. */
    @Multibinds
    abstract fun outboxHandlers(): Set<OutboxHandler>
}
