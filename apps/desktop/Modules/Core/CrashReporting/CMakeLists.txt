include(AedstCompilerWarnings)

# Local crash capture + consent-gated upload. Qt-free: it must keep working when
# the crash is inside Qt itself, and stays unit-testable with plain GoogleTest.
#
# The fatal-signal / minidump paths (crash_handler.cpp) cannot be unit-tested
# because they terminate the process. Manual verification checklist:
#   1. Call CrashHandler::install(<tmp>) early in main(), then deliver
#      SIGSEGV (raise/null deref), SIGABRT (abort) and, on Windows, an
#      unhandled SEH exception.
#   2. Expect exactly one new crash-<utc>-<pid>.txt (POSIX, with frames) or
#      .dmp (Windows) in <tmp>/crashes and the normal OS crash behaviour.
#   3. Run under `strace -f`/Process Monitor and confirm the handler only
#      write()s to the pre-opened descriptor (no open/malloc after the signal).
add_library(AedstCrashReporting STATIC
    include/aedst/crash/consent_store.hpp
    include/aedst/crash/crash_handler.hpp
    include/aedst/crash/crash_report.hpp
    include/aedst/crash/crash_uploader.hpp
    src/consent_store.cpp
    src/crash_handler.cpp
    src/crash_report.cpp
    src/crash_uploader.cpp
    src/file_util.hpp
)
add_library(Aedst::CrashReporting ALIAS AedstCrashReporting)

target_include_directories(AedstCrashReporting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstCrashReporting PUBLIC cxx_std_23)

# Aedst::Core brings Outcome/Clock/Logger and (PUBLIC) fmt.
target_link_libraries(AedstCrashReporting PUBLIC Aedst::Core)

if(WIN32)
    # MiniDumpWriteDump
    target_link_libraries(AedstCrashReporting PRIVATE dbghelp)
    target_compile_definitions(AedstCrashReporting PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

aedst_set_warnings(AedstCrashReporting)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
