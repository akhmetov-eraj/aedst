import QtQuick
import QtQuick.Controls.Basic
import QtTest

// AdminView.qml against the real AdminViewModel and a scripted repository: counters render, the access-denied state
// replaces the tabs, and the broadcast form only sends with a title.
TestCase {
    id: testCase
    name: "AdminView"
    width: 900
    height: 700
    visible: true
    when: windowShown

    function texts(item, out) {
        out = out || []
        if (item.text !== undefined && typeof item.text === "string" && item.text.length > 0) out.push(item.text)
        for (let i = 0; i < item.children.length; ++i) texts(item.children[i], out)
        return out
    }

    function find(item, name) {
        if (item.objectName === name) return item
        for (let i = 0; i < item.children.length; ++i) {
            const hit = find(item.children[i], name)
            if (hit) return hit
        }
        return null
    }

    function createView() {
        const component = Qt.createComponent(adminQmlDir + "/AdminView.qml")
        if (component.status !== Component.Ready) fail("AdminView.qml did not load: " + component.errorString())
        const view = component.createObject(testCase, { width: 900, height: 700 })
        verify(view !== null)
        return view
    }

    function init() {
        scriptedRepository.setForbidden(false)
        adminViewModel.clear()
    }

    function test_overviewCountersShowTheServerNumbers() {
        const view = createView()
        const shown = texts(view)
        verify(shown.indexOf("42") >= 0, "users total on the overview: " + shown.join("|"))
        verify(shown.indexOf("60") >= 0, "tasks done on the overview")
        view.destroy()
    }

    function test_aForbiddenAnswerShowsAccessDeniedAndHidesTheTabs() {
        scriptedRepository.setForbidden(true)
        const view = createView()
        verify(adminViewModel.forbidden)
        verify(texts(view).some(t => t.indexOf("Your account does not have the admin role") === 0))
        verify(!find(view, "adminTabs").visible)
        view.destroy()
    }

    function test_broadcastNeedsATitleBeforeItCanBeSent() {
        const view = createView()
        const title = find(view, "broadcastTitle")
        const send = find(view, "broadcastSend")
        verify(title !== null && send !== null)
        verify(!send.enabled, "no title yet")

        title.text = "Maintenance tonight"

        verify(send.enabled)
        view.destroy()
    }
}
