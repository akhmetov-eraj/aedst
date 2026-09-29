include(AedstCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml)

add_library(AedstDesignSystem STATIC
    include/aedst/designsystem/theme_controller.hpp
    src/theme_controller.cpp
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

# QML components ship as plain resources under qrc:/qt/qml/AedstDesignSystem —
# a default QQmlEngine import path since Qt 6.5 — with the hand-written qmldir,
# so `import AedstDesignSystem` works without extra import paths. Moving to
# qt_add_qml_module (generated qmldir + qmlcachegen) later is mechanical.
qt_add_resources(AedstDesignSystem aedst_designsystem_qml
    PREFIX "/qt/qml/AedstDesignSystem"
    BASE qml/AedstDesignSystem
    FILES
        qml/AedstDesignSystem/qmldir
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

aedst_set_warnings(AedstDesignSystem)
