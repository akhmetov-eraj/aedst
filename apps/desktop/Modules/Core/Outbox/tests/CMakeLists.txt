find_package(GTest REQUIRED)

# Store SQL on an in-memory database, the shared offline-outbox fixture replayed against the real drainer and the
# real SQLite queue (docs/contract/behavior/offline-outbox.json), and the Qt service (triggers, backoff, purge).
# One QCoreApplication for the binary; non-discovering like the other event-loop suites.
add_executable(AedstOutboxTests
    outbox_replay_test.cpp
    outbox_service_test.cpp
    outbox_store_test.cpp
    outbox_test_main.cpp
    outbox_test_support.hpp
)
target_link_libraries(AedstOutboxTests PRIVATE Aedst::Outbox Aedst::Testing GTest::gtest)
target_include_directories(AedstOutboxTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
aedst_set_warnings(AedstOutboxTests)

add_test(NAME AedstOutbox.OutboxTest COMMAND AedstOutboxTests)
set_tests_properties(AedstOutbox.OutboxTest PROPERTIES
    LABELS "unit"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
