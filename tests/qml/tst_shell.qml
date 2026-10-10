import QtQuick
import QtQuick.Window
import QtTest
import Nexora
import Nexora.Core

// Shell-level journeys with the real backend: section switching, sidebar
// focus, Back semantics, detail push/pop with focus restoration, modal
// containment, and the full launch -> running -> return lifecycle using the
// mock emulator.
Item {
    id: root
    width: 1920; height: 1080

    Shell { id: shell; anchors.fill: parent; focus: true }

    GameFilterModel { id: games; sourceModel: Library.model; sortMode: "title-asc" }
    SignalSpy { id: startedSpy; target: Launcher; signalName: "processStarted" }
    SignalSpy { id: finishedSpy; target: Launcher; signalName: "processFinished" }
    SignalSpy { id: failedSpy; target: Launcher; signalName: "launchFailed" }

    function activeItem() { return root.Window.window ? root.Window.window.activeFocusItem : null }
    function isInside(item, ancestor) {
        for (var p = item; p; p = p.parent) if (p === ancestor) return true
        return false
    }
    function modalCount() {
        var layer = null
        for (var i = 0; i < shell.children.length; ++i)
            if (shell.children[i].z === 100) layer = shell.children[i]
        var n = 0
        for (var j = 0; layer && j < layer.children.length; ++j) if (layer.children[j].visible) ++n
        return n
    }

    TestCase {
        name: "Shell"
        when: windowShown

        function initTestCase() {
            tryVerify(function() { return Library.hasScanned && games.count === 5 }, 10000, "library scanned")
            tryCompare(Router, "shell", shell)
            wait(100)
        }

        function cleanup() {
            // Close any modal left open and return to Home.
            for (var i = 0; i < 5 && modalCount() > 0; ++i) { keyClick(Qt.Key_Escape); wait(50) }
            tryVerify(function() { return Launcher.state === Launcher.Idle }, 10000)
            shell.showSection("home", true)
            shell.sectionHistory = []
            wait(50)
        }

        function test_startsOnHome() {
            compare(shell.currentSection, "home")
            verify(shell.currentPage !== null)
        }

        function test_sidebarNavigation() {
            shell.focusContent()
            wait(50)
            keyClick(Qt.Key_Left)
            tryVerify(function() { return shell.sidebarFocused }, 2000, "Left from content edge focuses sidebar")
            keyClick(Qt.Key_Down)
            tryCompare(shell, "currentSection", "library")
            keyClick(Qt.Key_Down)
            tryCompare(shell, "currentSection", "favorites")
            keyClick(Qt.Key_Right)
            tryVerify(function() { return !shell.sidebarFocused && isInside(activeItem(), shell.currentPage) },
                      2000, "Right enters the page")
        }

        function test_backFollowsHistoryAndNeverExits() {
            shell.showSection("emulators", true)
            shell.showSection("settings", true)
            wait(100)
            verify(shell.goBack())           // settings -> previous section
            compare(shell.currentSection, "emulators")
            wait(50)                         // let the deferred focus restore run
            shell.focusSidebar()
            wait(50)
            verify(shell.goBack())           // sidebar on non-home -> home
            compare(shell.currentSection, "home")
            wait(50)
            shell.focusSidebar()
            wait(50)
            verify(!shell.goBack())          // sidebar on home: nothing, app keeps running
            compare(shell.currentSection, "home")
        }

        function test_detailPushPopRestoresFocus() {
            shell.showSection("library", true)
            wait(300)
            var listPage = shell.currentPage
            var before = activeItem()
            verify(isInside(before, listPage), "focus is inside the library page")
            Router.push("GameDetailsPage", { gamePath: games.get(0).path })
            tryCompare(shell.currentStack, "depth", 2)
            tryVerify(function() { return isInside(activeItem(), shell.currentPage) && shell.currentPage !== listPage })
            keyClick(Qt.Key_Escape)
            tryCompare(shell.currentStack, "depth", 1)
            tryVerify(function() { return activeItem() === before }, 2000, "focus restored to the previously focused item")
        }

        function test_modalCapturesFocusAndBackCloses() {
            shell.focusContent()
            wait(50)
            var before = activeItem()
            var accepted = false
            Router.confirm({ title: "Test", message: "Body" }, function() { accepted = true })
            tryVerify(function() { return modalCount() === 1 }, 2000)
            keyClick(Qt.Key_Left)   // must not reach the sidebar
            verify(!shell.sidebarFocused)
            keyClick(Qt.Key_Escape)
            tryVerify(function() { return modalCount() === 0 })
            verify(!accepted)
            tryVerify(function() { return activeItem() === before })
        }

        function test_launchLifecycleUpdatesRecent() {
            startedSpy.clear(); finishedSpy.clear()
            var path = games.get(0).path
            Router.launchGame(path)
            verify(Launcher.busy)
            // A second launch while busy is refused without side effects.
            Router.launchGame(games.get(1).path)
            startedSpy.wait(10000)
            compare(Launcher.state, Launcher.Running)
            finishedSpy.wait(10000)
            compare(Launcher.lastResult, Launcher.NormalExit)
            compare(startedSpy.count, 1)
            compare(Library.mostRecentPath(), path)
            verify(Library.get(path).lastPlayed.getTime() > 0)
        }

        function test_unavailableEmulatorShowsError() {
            failedSpy.clear()
            var path = games.get(2).path
            Library.setEmulatorOverride(path, "missing")
            Router.launchGame(path)
            compare(Launcher.state, Launcher.Idle)
            tryVerify(function() { return modalCount() === 1 }, 2000, "explanation dialog shown")
            keyClick(Qt.Key_Escape)
            Library.setEmulatorOverride(path, "")
        }

        function test_compactLayoutAt720p() {
            var oldScale = Dimensions.scale
            Dimensions.scale = 720 / 1080
            Dimensions.compact = true
            root.width = 1280; root.height = 720
            wait(Motion.page + 50)
            for (var i = 0; i < Router.sections.length; ++i) {
                shell.showSection(Router.sections[i], true)
                wait(150)
                var page = shell.currentPage
                verify(page.width > 0 && page.width <= 1280, Router.sections[i] + " fits horizontally")
                verify(page.height > 0 && page.height <= 720, Router.sections[i] + " fits vertically")
            }
            Dimensions.compact = false
            Dimensions.scale = oldScale
            root.width = 1920; root.height = 1080
        }
    }
}
