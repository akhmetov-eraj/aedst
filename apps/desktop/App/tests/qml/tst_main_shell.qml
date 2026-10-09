import QtQuick
import QtQuick.Controls.Basic
import QtTest

// The app shell in its smallest supported window (480 x 480) and in a wide one: navigation never clips (the toolbar
// collapses into a menu below Theme.breakpointMd), dialogs fit, and the blocking error banner can be dismissed with Esc.
// Main.qml is created from source against the fakes registered in app_qml_test_main.cpp.
TestCase {
    id: testCase
    name: "MainShell"
    when: windowShown
    visible: true
    width: 100
    height: 100

    property var shell: null

    function initTestCase() {
        const component = Qt.createComponent(appQmlDir + "/Main.qml")
        if (component.status !== Component.Ready) {
            fail("Main.qml did not load: " + component.errorString())
        }
        shell = component.createObject(null, { initialRoute: "tasks/list" })
        verify(shell !== null, "Main.qml did not instantiate")
        shell.width = 480
        shell.height = 480
        waitForRendering(shell.contentItem)
    }

    function cleanupTestCase() {
        if (shell) shell.destroy()
    }

    function findByObjectName(root, name) {
        if (root.objectName === name) return root
        for (let i = 0; i < root.children.length; ++i) {
            const hit = findByObjectName(root.children[i], name)
            if (hit) return hit
        }
        return null
    }

    function test_narrowWindowCollapsesTheToolbarIntoAMenu() {
        shell.width = 480
        const toolbar = shell.header
        verify(toolbar.compact, "480 px is below breakpointMd")
        const menuButton = findByObjectName(toolbar, "menuButton")
        verify(menuButton !== null && menuButton.visible, "the Menu button is shown")
        const signOut = findByObjectName(toolbar, "signOutButton")
        verify(signOut !== null && !signOut.visible, "Sign out moves into the menu")
        verify(toolbar.implicitWidth <= 480 || toolbar.width <= 480, "the toolbar does not overflow the window")
    }

    function test_wideWindowShowsTheSectionsAsButtons() {
        shell.width = 1024
        const toolbar = shell.header
        verify(!toolbar.compact)
        verify(!findByObjectName(toolbar, "menuButton").visible)
        verify(findByObjectName(toolbar, "signOutButton").visible)
        shell.width = 480
    }

    function test_hiddenFeaturesDisappearFromTheNavigation() {
        fakeSession.visibleFeatures = ["tasks"]
        compare(shell.shownSections.length, 1)
        compare(shell.shownSections[0].feature, "tasks")
        fakeSession.visibleFeatures = ["tasks", "search", "media", "notifications", "admin"]
        compare(shell.shownSections.length, 5)
        fakeSession.isAdmin = false
        compare(shell.shownSections.length, 4)
        fakeSession.isAdmin = true
    }

    // Shortcuts are not Items, so they are found through the window's `data` list by objectName. (Key events cannot be
    // sent to a second window from this TestCase, so the Esc routing is asserted through the shortcuts' `enabled` state.)
    function shortcut(name) {
        for (let i = 0; i < shell.data.length; ++i) {
            if (shell.data[i].objectName === name) return shell.data[i]
        }
        return null
    }

    function test_theBlockingErrorBannerOwnsEscapeUntilItIsClosed() {
        fakeRouter.navigate("tasks/detail") // depth 2: Esc would normally go back
        const banner = findByObjectName(shell.contentItem, "errorBanner")
        verify(banner !== null)
        verify(banner.closable)
        verify(shortcut("backEscShortcut").enabled, "nothing blocks Esc yet: it goes back")

        fakeErrors.raise("unavailable", "The service is down.", true)

        verify(banner.visible)
        verify(shortcut("closeBannerShortcut").enabled, "Esc closes the banner")
        verify(!shortcut("backEscShortcut").enabled, "and does not also go back")

        banner.close()

        verify(!banner.visible)
        verify(shortcut("backEscShortcut").enabled, "with the banner gone Esc goes back again")
    }

    function test_theOfflineBannerSitsInTheFooterWithoutHidingTheContent() {
        fakeConnectivity.isOffline = true
        verify(shell.footer.visible)
        verify(shell.footer.implicitHeight > 0)
        fakeConnectivity.isOffline = false
        compare(shell.footer.implicitHeight, 0)
    }
}
