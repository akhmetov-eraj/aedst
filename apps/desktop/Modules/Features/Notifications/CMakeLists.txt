include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml Quick)

# BOUNDARY RULE: a feature links Core targets only - never Aedst::Feature::*.
# Native desktop notifications go through Core's platform::NotificationService
# (App injects the QSystemTrayIcon/D-Bus/in-app chain); links open by route name.

set(aedst_notifications_qml
    Presentation/NotificationsView.qml
    Presentation/NotificationPreferencesView.qml
)
foreach(qml_file IN LISTS aedst_notifications_qml)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()
qt_add_qml_module(AedstFeatureNotificationsViews
    STATIC
    URI Aedst.Feature.Notifications.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${aedst_notifications_qml}
    DEPENDENCIES QtQuick AedstDesignSystem
)

add_library(AedstFeatureNotifications STATIC
    Domain/notification.hpp
    Domain/notification_repository.hpp
    Domain/notification_stream.hpp
    Domain/new_notification_detector.hpp
    Domain/new_notification_detector.cpp
    Data/connect_notification_repository.hpp
    Data/connect_notification_repository.cpp
    Presentation/notification_list_model.hpp
    Presentation/notification_realtime.hpp
    Presentation/notification_realtime.cpp
    Presentation/notification_list_model.cpp
    Presentation/notifications_view_model.hpp
    Presentation/notifications_view_model.cpp
    Presentation/notifications_view_model_online.cpp
    Presentation/notifications_view_model_preferences.cpp
    Navigation/notifications_destinations.hpp
    Navigation/notifications_destinations.cpp
)
add_library(Aedst::Feature::Notifications ALIAS AedstFeatureNotifications)

# Declarative QML singletons (QML_ELEMENT + QML_SINGLETON, Aedst::QmlSupport); the module URI is the import name QML uses.
# Only App links AedstFeatureNotificationsplugin, so QML test executables keep registering their fakes under this URI.
target_link_libraries(AedstFeatureNotifications PUBLIC Aedst::QmlSupport)
qt_add_qml_module(AedstFeatureNotifications URI Aedst.Feature.Notifications VERSION 1.0 RESOURCE_PREFIX /qt/qml)

# The stream adapter needs the generated WatchNotifications messages: it is built once the server proto declares the RPC
# (docs/proposals/notifications-watch-rpc.md); until then the feature polls.
if(AEDST_HAS_WATCH_NOTIFICATIONS)
    target_sources(AedstFeatureNotifications PRIVATE
        Data/connect_notification_stream.hpp
        Data/connect_notification_stream.cpp)
endif()

target_include_directories(AedstFeatureNotifications PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AedstFeatureNotifications
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Aedst::Navigation
        Aedst::Plugin
        Qt6::Core
    PRIVATE
        Aedst::Analytics
        Aedst::Logging
        Aedst::Platform
        Qt6::Quick
)
target_link_libraries(AedstFeatureNotifications INTERFACE AedstFeatureNotificationsViewsplugin)

qt_add_translations(AedstFeatureNotifications
    TS_FILES
        i18n/notifications_en.ts
        i18n/notifications_ru.ts
        i18n/notifications_ar.ts
        i18n/notifications_tg.ts
        i18n/notifications_uz.ts
    SOURCES ${aedst_notifications_qml}
    LUPDATE_TARGET update_notifications_translations
    LRELEASE_TARGET release_notifications_translations
)

aedst_set_warnings(AedstFeatureNotifications)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
