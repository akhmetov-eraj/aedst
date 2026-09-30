find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

qt_add_executable(AedstConnectivityQtTests connectivity_monitor_test.cpp)
target_link_libraries(AedstConnectivityQtTests PRIVATE Aedst::Connectivity Qt6::Test)
aedst_set_warnings(AedstConnectivityQtTests)

add_test(NAME AedstConnectivity.ConnectivityMonitorTest COMMAND AedstConnectivityQtTests)
set_tests_properties(AedstConnectivity.ConnectivityMonitorTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
