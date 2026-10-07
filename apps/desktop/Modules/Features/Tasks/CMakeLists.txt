include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Qml Quick)

# BOUNDARY RULE: a feature links Core targets only - never Aedst::Feature::*.
# Attachments come from Aedst::Media (Core), not from Features/Media; opening an
# attachment pushes the media viewer *route name* (Aedst::Navigation). App/ is the
# only target that links every feature (docs/architecture-desktop.md).

# Views: a static QML module (qmlcachegen AOT, generated qmldir) - never plain
# qt_add_resources for production QML. The ViewModels they bind to are registered
# by App/ as singletons of `Aedst.Feature.Tasks` (they need injected dependencies).
set(aedst_tasks_qml
    Presentation/TaskListView.qml
    Presentation/TaskDetailView.qml
)
foreach(qml_file IN LISTS aedst_tasks_qml)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()
qt_add_qml_module(AedstFeatureTasksViews
    STATIC
    URI Aedst.Feature.Tasks.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${aedst_tasks_qml}
    DEPENDENCIES QtQuick AedstDesignSystem Aedst.Media
)

add_library(AedstFeatureTasks STATIC
    Domain/task.hpp
    Domain/task_repository.hpp
    Domain/create_task_use_case.hpp
    Domain/create_task_use_case.cpp
    Data/connect_task_repository.hpp
    Data/connect_task_repository.cpp
    Presentation/task_list_model.hpp
    Presentation/task_list_model.cpp
    Presentation/task_list_view_model.hpp
    Presentation/task_list_view_model.cpp
    Presentation/task_detail_view_model.hpp
    Presentation/task_detail_view_model.cpp
    Navigation/tasks_route.hpp
    Navigation/tasks_destinations.hpp
    Navigation/tasks_destinations.cpp
)
add_library(Aedst::Feature::Tasks ALIAS AedstFeatureTasks)

target_include_directories(AedstFeatureTasks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# PUBLIC where headers expose the type: Core (Outcome/ErrorSink), Network +
# Contract (Data speaks generated messages), Navigation (Route), Media
# (TaskDetailViewModel exposes MediaListModel / MediaUploadController).
target_link_libraries(AedstFeatureTasks
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Aedst::Navigation
        Aedst::Media
        Qt6::Core
    PRIVATE
        Aedst::Analytics
        Qt6::Quick
)

# Static QML module: whoever links the feature (App, QML tests) must also link the
# plugin so its resources and type registrations are not dropped by the linker.
target_link_libraries(AedstFeatureTasks INTERFACE AedstFeatureTasksViewsplugin)

qt_add_translations(AedstFeatureTasks
    TS_FILES
        i18n/tasks_en.ts
        i18n/tasks_ru.ts
        i18n/tasks_ar.ts
        i18n/tasks_tg.ts
        i18n/tasks_uz.ts
    SOURCES ${aedst_tasks_qml}
    LUPDATE_TARGET update_tasks_translations
    LRELEASE_TARGET release_tasks_translations
)

aedst_set_warnings(AedstFeatureTasks)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
