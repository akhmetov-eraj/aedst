find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test Network)

# Both ViewModels against a scripted Core MediaRemoteDataSource (Qt Test: they poll with QTimer).
qt_add_executable(AedstFeatureMediaTests media_view_models_test.cpp)
target_link_libraries(AedstFeatureMediaTests PRIVATE Aedst::Feature::Media Aedst::Analytics Qt6::Test Qt6::Network)
aedst_set_warnings(AedstFeatureMediaTests)

add_test(NAME AedstFeatureMedia.MediaViewModelsTest COMMAND AedstFeatureMediaTests)
set_tests_properties(AedstFeatureMedia.MediaViewModelsTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
