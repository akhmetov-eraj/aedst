import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AedstDesignSystem
import Aedst.Navigation
import Aedst.Feature.Auth

// App shell: one StackView mirrored from the Core Router's back stack — the
// desktop counterpart of apple's RootView.swift and android's AppNavHost.kt.
// Screens come from each feature's registered destinations; this file only
// knows route names, plus the app-level chrome (toolbar, sign out).
ApplicationWindow {
    id: window

    required property string initialRoute

    width: 1024
    height: 720
    minimumWidth: 480
    minimumHeight: 480
    visible: true
    title: qsTr("AEDST")
    color: Theme.background

    header: ToolBar {
        visible: AppRouter.depth > 0 && AppRouter.currentRoute !== "auth/login"

        background: Rectangle { color: Theme.surface }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing
            anchors.rightMargin: Theme.spacing

            ToolButton {
                visible: AppRouter.depth > 1
                text: qsTr("Back")
                onClicked: AppRouter.goBack()
            }

            Label {
                Layout.fillWidth: true
                text: window.title
                font: Theme.bodyFont
                color: Theme.onSurface
                elide: Text.ElideRight
            }

            ToolButton {
                text: qsTr("Sign out")
                enabled: !LoginViewModel.isLoading
                onClicked: LoginViewModel.logout()
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
    }

    Connections {
        target: AppRouter

        function onPushRequested(view, properties) {
            stack.push(view, properties)
        }

        function onPopRequested() {
            stack.pop()
        }

        function onReplaceAllRequested(view, properties) {
            stack.clear()
            stack.push(view, properties)
        }
    }

    Component.onCompleted: AppRouter.replaceAll(initialRoute)
}
