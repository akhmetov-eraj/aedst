include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Qml)

# QML files of the module. `AedstTokens.qml` is GENERATED from packages/design-tokens
# (`pnpm --dir packages/design-tokens build`); never edit it by hand.
set(AEDST_DESIGNSYSTEM_QML_FILES
    # Tokens + theme facade (singletons, see below)
    qml/AedstDesignSystem/AedstTokens.qml
    qml/AedstDesignSystem/Theme.qml
    # Layout
    qml/AedstDesignSystem/AedstCard.qml
    qml/AedstDesignSystem/AedstSeparator.qml
    qml/AedstDesignSystem/AedstAspectRatio.qml
    qml/AedstDesignSystem/AedstScrollArea.qml
    qml/AedstDesignSystem/AedstCollapsible.qml
    # Actions
    qml/AedstDesignSystem/AedstButton.qml
    qml/AedstDesignSystem/AedstIconButton.qml
    qml/AedstDesignSystem/AedstToggleButton.qml
    qml/AedstDesignSystem/AedstToggleGroup.qml
    # Forms
    qml/AedstDesignSystem/AedstTextField.qml
    qml/AedstDesignSystem/AedstTextArea.qml
    qml/AedstDesignSystem/AedstLabel.qml
    qml/AedstDesignSystem/AedstCheckBox.qml
    qml/AedstDesignSystem/AedstSwitch.qml
    qml/AedstDesignSystem/AedstRadioButton.qml
    qml/AedstDesignSystem/AedstRadioGroup.qml
    qml/AedstDesignSystem/AedstComboBox.qml
    qml/AedstDesignSystem/AedstSearchableComboBox.qml
    qml/AedstDesignSystem/AedstSlider.qml
    qml/AedstDesignSystem/AedstOtpInput.qml
    # Feedback
    qml/AedstDesignSystem/AedstAlert.qml
    qml/AedstDesignSystem/AedstAlertDialog.qml
    qml/AedstDesignSystem/AedstToast.qml
    qml/AedstDesignSystem/AedstProgressBar.qml
    qml/AedstDesignSystem/AedstCircularProgress.qml
    qml/AedstDesignSystem/AedstSkeleton.qml
    qml/AedstDesignSystem/AedstBadge.qml
    qml/AedstDesignSystem/AedstToolTip.qml
    # Overlay / navigation
    qml/AedstDesignSystem/AedstDialog.qml
    qml/AedstDesignSystem/AedstDrawer.qml
    qml/AedstDesignSystem/AedstPopover.qml
    qml/AedstDesignSystem/AedstDropdownMenu.qml
    qml/AedstDesignSystem/AedstContextMenu.qml
    qml/AedstDesignSystem/AedstMenuBar.qml
    qml/AedstDesignSystem/AedstTabBar.qml
    qml/AedstDesignSystem/AedstSearchableList.qml
    qml/AedstDesignSystem/AedstHoverCard.qml
    # Data display
    qml/AedstDesignSystem/AedstAvatar.qml
    qml/AedstDesignSystem/AedstTableView.qml
    qml/AedstDesignSystem/AedstCalendar.qml
    qml/AedstDesignSystem/AedstCarousel.qml
    qml/AedstDesignSystem/AedstAccordion.qml
    qml/AedstDesignSystem/AedstBreadcrumb.qml
    qml/AedstDesignSystem/AedstPagination.qml
    qml/AedstDesignSystem/AedstChart.qml
)

# Files with `pragma Singleton` must be flagged so the generated qmldir gets a `singleton` entry.
set_source_files_properties(
    qml/AedstDesignSystem/AedstTokens.qml
    qml/AedstDesignSystem/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

# The files live in qml/AedstDesignSystem/ on disk. Alias each to its bare file name so the
# module resource path is qrc:/qt/qml/AedstDesignSystem/<File>.qml instead of
# .../AedstDesignSystem/qml/AedstDesignSystem/<File>.qml. QT_RESOURCE_ALIAS must be set
# before the files are handed to qt_add_qml_module.
foreach(qml_file IN LISTS AEDST_DESIGNSYSTEM_QML_FILES)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()

# qt_add_qml_module (instead of plain qt_add_resources) generates the qmldir and runs
# qmlcachegen, so components ship as ahead-of-time compiled bytecode instead of being parsed at
# every launch (apps/desktop/docs/performance.md, "Compile QML ahead of time").
# STATIC: the backing library is linked into the app; Qt then also generates the static QML
# plugin target `AedstDesignSystemplugin`, which is linked below.
qt_add_qml_module(AedstDesignSystem
    STATIC
    URI AedstDesignSystem
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        include/aedst/designsystem/theme_controller.hpp
        src/theme_controller.cpp
    QML_FILES ${AEDST_DESIGNSYSTEM_QML_FILES}
)
add_library(Aedst::DesignSystem ALIAS AedstDesignSystem)

target_include_directories(AedstDesignSystem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(AedstDesignSystem
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        Qt6::Qml
)

# A static QML module's plugin must be linked by whoever imports the module, otherwise the
# linker may drop the module's resources / type registration. Attach it to the backing target so
# every consumer of Aedst::DesignSystem (App, Auth) gets it without touching their CMake.
# The plugin already links the backing library; CMake allows cycles between static libraries.
target_link_libraries(AedstDesignSystem INTERFACE AedstDesignSystemplugin)

aedst_set_warnings(AedstDesignSystem)
