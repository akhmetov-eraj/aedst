include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml Quick)

# BOUNDARY RULE: a feature links Core targets only - never Aedst::Feature::*.
# A hit is opened with Router::openLink(<server link>), i.e. by route name; this
# feature never sees Tasks/Media types.

set(aedst_search_qml Presentation/SearchView.qml)
foreach(qml_file IN LISTS aedst_search_qml)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()
qt_add_qml_module(AedstFeatureSearchViews
    STATIC
    URI Aedst.Feature.Search.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${aedst_search_qml}
    DEPENDENCIES QtQuick AedstDesignSystem
)

add_library(AedstFeatureSearch STATIC
    Domain/search_hit.hpp
    Domain/search_repository.hpp
    Domain/snippet.hpp
    Domain/snippet.cpp
    Data/connect_search_repository.hpp
    Data/connect_search_repository.cpp
    Presentation/search_results_model.hpp
    Presentation/search_results_model.cpp
    Presentation/search_view_model.hpp
    Presentation/search_view_model.cpp
    Navigation/search_destinations.hpp
    Navigation/search_destinations.cpp
)
add_library(Aedst::Feature::Search ALIAS AedstFeatureSearch)

target_include_directories(AedstFeatureSearch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(AedstFeatureSearch
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
target_link_libraries(AedstFeatureSearch INTERFACE AedstFeatureSearchViewsplugin)

qt_add_translations(AedstFeatureSearch
    TS_FILES
        i18n/search_en.ts
        i18n/search_ru.ts
        i18n/search_ar.ts
        i18n/search_tg.ts
        i18n/search_uz.ts
    SOURCES ${aedst_search_qml}
    LUPDATE_TARGET update_search_translations
    LRELEASE_TARGET release_search_translations
)

aedst_set_warnings(AedstFeatureSearch)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
