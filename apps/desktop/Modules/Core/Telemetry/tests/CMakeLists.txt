find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# QObjects (signals, QNetworkAccessManager construction) need a QCoreApplication:
# AEDST_GTEST_MAIN_WITH_QCOREAPPLICATION from Aedst::Testing. No network I/O happens
# (nothing is flushed while consent is off / batches stay below maxBatchSize).
add_executable(AedstTelemetryTests telemetry_test.cpp)
target_link_libraries(AedstTelemetryTests PRIVATE Aedst::Telemetry Aedst::Testing Qt6::Test GTest::gtest)
aedst_set_warnings(AedstTelemetryTests)

add_test(NAME AedstTelemetry.TelemetryTest COMMAND AedstTelemetryTests)
set_tests_properties(AedstTelemetry.TelemetryTest PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
