include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml Quick)

# BOUNDARY RULE: a feature links Core targets only - never Aedst::Feature::*.
# The reusable media machinery (upload pipeline, image provider, player, thumbnails,
# compression) is Core (Aedst::Media) so Tasks can attach files without linking this
# feature; this feature only owns the library and viewer screens.

set(aedst_media_feature_qml
    Presentation/MediaLibraryView.qml
    Presentation/MediaViewerView.qml
)
foreach(qml_file IN LISTS aedst_media_feature_qml)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()
qt_add_qml_module(AedstFeatureMediaViews
    STATIC
    URI Aedst.Feature.Media.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${aedst_media_feature_qml}
    DEPENDENCIES QtQuick AedstDesignSystem Aedst.Media
)

add_library(AedstFeatureMedia STATIC
    Presentation/media_library_view_model.hpp
    Presentation/media_library_view_model.cpp
    Presentation/media_viewer_view_model.hpp
    Presentation/media_viewer_view_model.cpp
    Navigation/media_destinations.hpp
    Navigation/media_destinations.cpp
)
add_library(Aedst::Feature::Media ALIAS AedstFeatureMedia)

# Declarative QML singletons (QML_ELEMENT + QML_SINGLETON, Aedst::QmlSupport); the module URI is the import name QML uses.
# Only App links AedstFeatureMediaplugin, so QML test executables keep registering their fakes under this URI.
target_link_libraries(AedstFeatureMedia PUBLIC Aedst::QmlSupport)
qt_add_qml_module(AedstFeatureMedia URI Aedst.Feature.Media VERSION 1.0 RESOURCE_PREFIX /qt/qml)

target_include_directories(AedstFeatureMedia PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AedstFeatureMedia
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Aedst::Navigation
        Aedst::Plugin
        Aedst::Media
        Qt6::Core
    PRIVATE
        Aedst::Analytics
        Qt6::Quick
)
target_link_libraries(AedstFeatureMedia INTERFACE AedstFeatureMediaViewsplugin)

qt_add_translations(AedstFeatureMedia
    TS_FILES
        i18n/media_en.ts
        i18n/media_ru.ts
        i18n/media_ar.ts
        i18n/media_tg.ts
        i18n/media_uz.ts
    SOURCES ${aedst_media_feature_qml}
    LUPDATE_TARGET update_media_translations
    LRELEASE_TARGET release_media_translations
)

aedst_set_warnings(AedstFeatureMedia)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
