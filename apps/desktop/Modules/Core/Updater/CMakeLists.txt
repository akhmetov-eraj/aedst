include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Network)

# Self-update orchestration: signed manifest (Aedst::Update) -> bind to the installer's own feed -> download ->
# verify -> install on exit. The installer is a port (UpdateInstaller); Velopack is the production adapter. See
# docs/adr/0029-delivery-updates-signing.md.
add_library(AedstUpdater STATIC
    include/aedst/updater/qt_http_fetcher.hpp
    include/aedst/updater/update_installer.hpp
    include/aedst/updater/update_pipeline.hpp
    include/aedst/updater/update_service.hpp
    src/qt_http_fetcher.cpp
    src/update_pipeline.cpp
    src/update_service.cpp
)
add_library(Aedst::Updater ALIAS AedstUpdater)

target_include_directories(AedstUpdater PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(AedstUpdater
    PUBLIC
        Aedst::Core
        Aedst::Update
        Qt6::Core
    PRIVATE
        Aedst::Logging
        Qt6::Network
)

option(AEDST_ENABLE_VELOPACK "Build the Velopack installer adapter (self-update for installed builds)" OFF)
if(AEDST_ENABLE_VELOPACK)
    include(AedstVelopack)
    aedst_fetch_velopack()
    target_sources(AedstUpdater PRIVATE src/velopack_installer.hpp src/velopack_installer.cpp)
    target_include_directories(AedstUpdater PRIVATE "${AEDST_VELOPACK_INCLUDE_DIR}")
    target_link_libraries(AedstUpdater PRIVATE "${AEDST_VELOPACK_LIBRARY}")
    # The App includes velopack_installer.hpp through this directory and calls velopackStartup() first thing in main().
    target_include_directories(AedstUpdater PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_compile_definitions(AedstUpdater PUBLIC AEDST_HAS_VELOPACK=1)
    if(AEDST_VELOPACK_RUNTIME)
        install(PROGRAMS "${AEDST_VELOPACK_RUNTIME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    else()
        message(WARNING "AedstUpdater: the Velopack runtime library was not found; the package would not start")
    endif()
    message(STATUS "AedstUpdater: Velopack ${AEDST_VELOPACK_VERSION} enabled")
else()
    message(STATUS "AedstUpdater: Velopack disabled; updates are verified and announced but not installed by the app")
endif()

aedst_set_warnings(AedstUpdater)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
