find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)
include(GoogleTest)

# The update tests build signed manifests with throw-away Ed25519 keys; the helpers live next to the Aedst::Update tests.
set(aedst_update_test_support_dir "${PROJECT_SOURCE_DIR}/Modules/Core/Update/tests")

# The pipeline is plain blocking C++ against fakes (no Qt event loop).
add_executable(AedstUpdaterPipelineTests update_pipeline_test.cpp)
target_link_libraries(AedstUpdaterPipelineTests PRIVATE Aedst::Updater OpenSSL::Crypto GTest::gtest_main)
target_include_directories(AedstUpdaterPipelineTests PRIVATE "${aedst_update_test_support_dir}")
aedst_set_warnings(AedstUpdaterPipelineTests)
gtest_discover_tests(AedstUpdaterPipelineTests
    TEST_PREFIX "AedstUpdater."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit")

# The service runs the pipeline on a QThreadPool thread and talks to the GUI thread: needs a QCoreApplication.
add_executable(AedstUpdaterServiceTests update_service_test.cpp)
target_link_libraries(AedstUpdaterServiceTests PRIVATE Aedst::Updater Aedst::Testing OpenSSL::Crypto Qt6::Test GTest::gtest)
target_include_directories(AedstUpdaterServiceTests PRIVATE "${aedst_update_test_support_dir}")
aedst_set_warnings(AedstUpdaterServiceTests)
add_test(NAME AedstUpdater.ServiceTest COMMAND AedstUpdaterServiceTests)
set_tests_properties(AedstUpdater.ServiceTest PROPERTIES
    LABELS "unit"
    TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
