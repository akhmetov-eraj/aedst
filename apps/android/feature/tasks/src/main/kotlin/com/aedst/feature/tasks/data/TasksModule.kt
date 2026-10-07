package com.aedst.feature.tasks.data

import com.aedst.feature.tasks.domain.TaskRepository
import com.aedst.tasks.v1.TasksServiceClient
import com.aedst.tasks.v1.TasksServiceClientInterface
import com.connectrpc.ProtocolClientInterface
import dagger.Binds
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class TasksModule {

    @Binds
    abstract fun bindTaskRepository(impl: ConnectTaskRepository): TaskRepository

    companion object {
        @Provides
        @Singleton
        fun provideTasksServiceClient(client: ProtocolClientInterface): TasksServiceClientInterface =
            TasksServiceClient(client)
    }
}
