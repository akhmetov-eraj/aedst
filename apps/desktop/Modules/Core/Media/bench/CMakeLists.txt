find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Gui Qml Quick)

# Scroll smoothness and memory of the real MediaGrid over 5 000 async-image tiles. Prints one JSON line; scripts/perf/run-perf.mjs
# compares it with perf/budgets.json. Frame times need a real window system (the CI perf job runs it under xvfb on Linux
# and on the Windows desktop session); the `bench` CTest label keeps it out of normal `ctest` runs.
qt_add_executable(AedstMediaScrollBench scroll_bench.cpp)
target_link_libraries(AedstMediaScrollBench PRIVATE Aedst::Media Aedst::DesignSystem Qt6::Gui Qt6::Qml Qt6::Quick)
aedst_set_warnings(AedstMediaScrollBench)

add_test(NAME AedstMedia.ScrollBench COMMAND AedstMediaScrollBench)
set_tests_properties(AedstMedia.ScrollBench PROPERTIES LABELS "bench" TIMEOUT 180
    ENVIRONMENT "QT_QUICK_CONTROLS_STYLE=Basic")
