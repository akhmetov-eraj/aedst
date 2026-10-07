include(AedstCompilerWarnings)

# Widgets: QSystemTrayIcon (TrayNotificationService); needs a QApplication at runtime.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Network Widgets)
# Optional: only used for native Linux desktop notifications
# (org.freedesktop.Notifications). Without it the in-app fallback is used.
find_package(Qt6 ${AEDST_QT_MINIMUM} QUIET OPTIONAL_COMPONENTS DBus)

# Per-OS capabilities behind interfaces (single instance, autostart,
# notifications, platform info). Selection is compile-time (#if _WIN32 /
# __APPLE__ / else) plus a runtime availability check. Pure logic (deep-link
# validation, path/format rendering, platform detection) is Qt-free.
add_library(AedstPlatform STATIC
    include/aedst/platform/autostart.hpp
    include/aedst/platform/deep_link.hpp
    include/aedst/platform/notification_service.hpp
    include/aedst/platform/platform_info.hpp
    include/aedst/platform/single_instance_guard.hpp
    include/aedst/platform/tray_notification_service.hpp
    src/autostart.cpp
    src/deep_link.cpp
    src/notification_service.cpp
    src/platform_info.cpp
    src/single_instance_guard.cpp
    src/tray_notification_service.cpp
)
add_library(Aedst::Platform ALIAS AedstPlatform)

target_include_directories(AedstPlatform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstPlatform PUBLIC cxx_std_23)

# PUBLIC: SingleInstanceGuard / InAppNotificationService are QObjects and the
# guard uses QLocalServer (Qt6::Network); Outcome comes from Aedst::Core.
target_link_libraries(AedstPlatform PUBLIC Aedst::Core Qt6::Core Qt6::Network PRIVATE Qt6::Gui Qt6::Widgets)

if(UNIX AND NOT APPLE AND TARGET Qt6::DBus)
    target_sources(AedstPlatform PRIVATE
        src/dbus_notification_service.hpp
        src/dbus_notification_service.cpp)
    target_link_libraries(AedstPlatform PRIVATE Qt6::DBus)
    target_compile_definitions(AedstPlatform PRIVATE AEDST_PLATFORM_HAS_DBUS)
    message(STATUS "AedstPlatform: native Linux notifications enabled (Qt6::DBus)")
elseif(UNIX AND NOT APPLE)
    message(STATUS "AedstPlatform: Qt6::DBus not found; notifications use the in-app fallback only")
endif()

if(WIN32)
    target_compile_definitions(AedstPlatform PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

aedst_set_warnings(AedstPlatform)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
