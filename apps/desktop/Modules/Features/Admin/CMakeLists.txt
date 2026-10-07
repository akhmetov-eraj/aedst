include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml Quick)

# BOUNDARY RULE: a feature links Core targets only - never Aedst::Feature::*.
# Visibility of this feature is decided in App/ from the access token's roles
# (network::parseAccessTokenClaims); the server enforces the role on every RPC.

set(aedst_admin_qml Presentation/AdminView.qml)
foreach(qml_file IN LISTS aedst_admin_qml)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()
qt_add_qml_module(AedstFeatureAdminViews
    STATIC
    URI Aedst.Feature.Admin.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${aedst_admin_qml}
    DEPENDENCIES QtQuick AedstDesignSystem
)

add_library(AedstFeatureAdmin STATIC
    Data/connect_admin_repository.hpp
    Data/connect_admin_repository.cpp
    Presentation/admin_rows_model.hpp
    Presentation/admin_rows_model.cpp
    Presentation/admin_view_model.hpp
    Presentation/admin_view_model.cpp
    Navigation/admin_destinations.hpp
    Navigation/admin_destinations.cpp
)
add_library(Aedst::Feature::Admin ALIAS AedstFeatureAdmin)

target_include_directories(AedstFeatureAdmin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AedstFeatureAdmin
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Aedst::Navigation
        Qt6::Core
    PRIVATE
        Aedst::Analytics
        Qt6::Quick
)
target_link_libraries(AedstFeatureAdmin INTERFACE AedstFeatureAdminViewsplugin)

qt_add_translations(AedstFeatureAdmin
    TS_FILES
        i18n/admin_en.ts
        i18n/admin_ru.ts
        i18n/admin_ar.ts
        i18n/admin_tg.ts
        i18n/admin_uz.ts
    SOURCES ${aedst_admin_qml}
    LUPDATE_TARGET update_admin_translations
    LRELEASE_TARGET release_admin_translations
)

aedst_set_warnings(AedstFeatureAdmin)
