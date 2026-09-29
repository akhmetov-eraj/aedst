# Every module registers its own tests next to its code via
# `add_subdirectory(tests)` (see Modules/**/tests/CMakeLists.txt), labelled:
#   unit    — GoogleTest, pure C++ (Domain/Data/use cases)
#   qttest  — Qt Test, QObject-based code (ViewModels)
# This directory only adds label-scoped convenience targets on top of CTest;
# it deliberately holds no test sources.

add_custom_target(check
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> --output-on-failure
    USES_TERMINAL
    COMMENT "Running all CTest tests")

add_custom_target(check-unit
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L unit --output-on-failure
    USES_TERMINAL
    COMMENT "Running GoogleTest (label: unit)")

add_custom_target(check-qttest
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L qttest --output-on-failure
    USES_TERMINAL
    COMMENT "Running Qt Test (label: qttest)")
