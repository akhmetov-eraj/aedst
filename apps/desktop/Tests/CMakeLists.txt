# Every module registers its own tests next to its code via
# `add_subdirectory(tests)` (see Modules/**/tests/CMakeLists.txt), labelled:
#   unit    — GoogleTest, pure C++ (Domain/Data/use cases)
#   qttest  — Qt Test, QObject-based code (ViewModels)
#   qmltest — QtQuickTest (tst_*.qml), run headless with QT_QPA_PLATFORM=offscreen
# This directory only adds label-scoped convenience targets on top of CTest;
# it deliberately holds no test sources.

add_custom_target(check
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> --output-on-failure
    USES_TERMINAL
    VERBATIM
    COMMENT "Running all CTest tests")

add_custom_target(check-unit
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L unit --output-on-failure
    USES_TERMINAL
    VERBATIM
    COMMENT "Running GoogleTest (label: unit)")

add_custom_target(check-qttest
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L qttest --output-on-failure
    USES_TERMINAL
    VERBATIM
    COMMENT "Running Qt Test (label: qttest)")

add_custom_target(check-qmltest
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> -L qmltest --output-on-failure
    USES_TERMINAL
    VERBATIM
    COMMENT "Running QtQuickTest (label: qmltest)")

# --- Sanitizers -----------------------------------------------------------------
# Configure with a sanitizer preset (gcc-asan, clang-asan, clang-tsan, msvc-asan,
# macos-asan) and prefer `ctest --preset <name>`: the test presets export the same
# environment. This target does the equivalent for a hand-configured tree.
if(NOT AEDST_SANITIZER STREQUAL "none")
    add_custom_target(check-sanitize
        COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen ${AEDST_SANITIZER_RUN_ENV}
                "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> --output-on-failure
        USES_TERMINAL
        VERBATIM
        COMMENT "Running all tests under AEDST_SANITIZER=${AEDST_SANITIZER}")
else()
    add_custom_target(check-sanitize
        COMMAND "${CMAKE_COMMAND}" -E echo
                "check-sanitize: this tree is not sanitized. Configure with a preset such as gcc-asan, clang-asan, clang-tsan, msvc-asan or macos-asan (or -DAEDST_SANITIZER=asan-ubsan|tsan)."
        COMMENT "check-sanitize hint"
        VERBATIM)
endif()

# --- Coverage (gcovr) -------------------------------------------------------------
# `cmake --preset coverage && cmake --build --preset coverage && #  cmake --build --preset coverage --target coverage`
# Writes <build>/coverage/{coverage.xml (Cobertura), index.html (details), summary}.
# First-party sources only: Modules/ and App/, minus tests and moc/rcc/proto output.
if(AEDST_COVERAGE)
    find_program(AEDST_GCOVR gcovr)
    if(AEDST_GCOVR)
        set(aedst_cov_dir "${CMAKE_BINARY_DIR}/coverage")
        add_custom_target(coverage
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${aedst_cov_dir}"
            COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                    "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C $<CONFIG> --output-on-failure
            COMMAND "${AEDST_GCOVR}"
                    --root "${PROJECT_SOURCE_DIR}"
                    --object-directory "${CMAKE_BINARY_DIR}"
                    --filter "${PROJECT_SOURCE_DIR}/Modules/"
                    --filter "${PROJECT_SOURCE_DIR}/App/"
                    --exclude ".*/tests/.*"
                    --exclude ".*/bench/.*"
                    --exclude ".*_autogen.*"
                    --exclude-throw-branches
                    --exclude-unreachable-branches
                    --print-summary
                    --xml "${aedst_cov_dir}/coverage.xml"
                    --html-details "${aedst_cov_dir}/index.html"
            USES_TERMINAL
            VERBATIM
            COMMENT "Running tests and generating gcovr coverage report in ${aedst_cov_dir}")
    else()
        message(STATUS "AEDST: gcovr not found; `coverage` target disabled (pip install gcovr)")
    endif()
endif()
