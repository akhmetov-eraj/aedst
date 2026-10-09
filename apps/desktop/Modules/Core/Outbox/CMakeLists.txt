include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# Offline write queue (the desktop counterpart of Android's core/sync, ADR-0013): a SQLite `outbox` table (migration 2
# in Core/Persistence, applied by the DatabaseWorker), a framework-free FIFO drainer, and a QObject service that
# triggers it (enqueue, network back, backoff) and exposes the pending count. Features contribute OutboxHandlers
# through the plugin ServiceRegistry; this module never knows a feature. Core module: no dependency on any feature.
add_library(AedstOutbox STATIC
    include/aedst/outbox/outbox_drainer.hpp
    include/aedst/outbox/outbox_handler.hpp
    include/aedst/outbox/outbox_item.hpp
    include/aedst/outbox/outbox_queue.hpp
    include/aedst/outbox/outbox_service.hpp
    include/aedst/outbox/outbox_store.hpp
    src/outbox_drainer.cpp
    src/outbox_service.cpp
    src/outbox_store.cpp
    src/sqlite_outbox_queue.cpp
)
add_library(Aedst::Outbox ALIAS AedstOutbox)

target_include_directories(AedstOutbox PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# Network: RetryPolicy (the shared exponential backoff) appears in OutboxService's public header.
target_link_libraries(AedstOutbox PUBLIC Aedst::Core Aedst::Network Aedst::Persistence Qt6::Core PRIVATE Aedst::Logging)

aedst_set_warnings(AedstOutbox)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
