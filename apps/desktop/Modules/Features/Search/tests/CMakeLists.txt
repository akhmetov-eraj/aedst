find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Test)

# Snippet parsing/escaping (pure) + the ViewModel's debounce and stale-response
# guard (Qt Test: needs an event loop for the QTimer).
qt_add_executable(AedstFeatureSearchTests search_test.cpp)
target_link_libraries(AedstFeatureSearchTests PRIVATE Aedst::Feature::Search Qt6::Test)
aedst_set_warnings(AedstFeatureSearchTests)

add_test(NAME AedstFeatureSearch.SearchTest COMMAND AedstFeatureSearchTests)
set_tests_properties(AedstFeatureSearch.SearchTest PROPERTIES
    LABELS "qttest"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
