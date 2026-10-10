import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Nexora.Core

// Global application shell: sidebar + per-section page stacks + hint bar,
// plus the overlay layers (launch overlay, modals, toasts).
//
// Each top-level section owns a StackView created on first visit and kept
// alive, so switching sections preserves scroll position and focus.
//
// Back behaviour (in order):
//   1. an open modal handles Back itself (closes)
//   2. a page may accept Back first (e.g. leave a search field / settings panel)
//   3. pop a detail page off the current section stack (focus restored)
//   4. on a top-level page with focus in content: go to the previous section
//      in history, else move focus to the sidebar
//   5. on the sidebar: return to Home; on Home's sidebar nothing happens —
//      Back never exits the application.
FocusScope {
    id: shell

    property string currentSection: "home"
    property var sectionHistory: []
    readonly property var sectionPages: ({
        home: "HomePage", library: "GameLibraryPage", favorites: "FavoritesPage",
        emulators: "EmulatorsPage", settings: "SettingsPage"
    })
    // Updated by the visible section holder (Repeater.itemAt() is not reactive).
    property StackView currentStack: null
    readonly property Item currentPage: currentStack ? currentStack.currentItem : null
    readonly property bool sidebarFocused: sidebar.activeFocus
    readonly property bool compactLayout: Dimensions.compact || Settings.sidebarCompact

    Component.onCompleted: {
        Router.shell = shell
        showSection("home", true)
    }
    Component.onDestruction: if (Router.shell === shell) Router.shell = null

    // ---- navigation API (called through Router) -------------------------
    function showSection(name, focusContent) {
        var index = Router.sections.indexOf(name)
        if (index < 0) return
        if (name !== currentSection) {
            sectionHistory = sectionHistory.filter(function(s) { return s !== currentSection }).concat([currentSection]).slice(-8)
            currentSection = name
        }
        var holder = stacks.itemAt(index)
        holder.ensureLoaded()
        if (focusContent) Qt.callLater(shell.focusContent)
    }
    function pushPage(page, properties) {
        if (!currentStack) return
        currentStack.push(Qt.resolvedUrl(page + ".qml"), properties)
        Qt.callLater(shell.focusContent)
    }
    function focusSidebar() { sidebar.focusActive() }
    function focusContent() {
        var page = currentPage
        if (page && page.restoreFocus) page.restoreFocus()
        else if (page) page.forceActiveFocus()
    }
    function goBack() {
        if (currentStack && currentStack.depth > 1) {
            currentStack.pop()
            Qt.callLater(shell.focusContent)
            return true
        }
        if (!sidebar.activeFocus) {
            if (currentSection !== "home" && sectionHistory.length > 0) {
                var previous = sectionHistory[sectionHistory.length - 1]
                sectionHistory = sectionHistory.slice(0, -1)
                currentSection = previous
                stacks.itemAt(Router.sections.indexOf(previous)).ensureLoaded()
                Qt.callLater(shell.focusContent)
            } else {
                focusSidebar()
            }
            return true
        }
        if (currentSection !== "home") {
            sectionHistory = []
            currentSection = "home"
            stacks.itemAt(0).ensureLoaded()
            Qt.callLater(focusSidebar)
            return true
        }
        return false
    }
    function openSettings(category) {
        showSection("settings", true)
        var holder = stacks.itemAt(Router.sections.indexOf("settings"))
        if (holder.stack.depth > 1) holder.stack.pop(null)
        Qt.callLater(function() {
            var page = holder.stack.get(0)
            if (page && page.openCategory) page.openCategory(category)
        })
    }

    // ---- feedback API ---------------------------------------------------
    function showToast(message, kind) { toastHost.show(message, kind) }
    function _createModal(component, properties) {
        var obj = component.createObject(modalLayer, properties)
        if (!obj) console.warn("Nexora: failed to create modal", component.errorString())
        return obj
    }
    function openConfirm(options, onConfirm, onCancel) {
        var d = _createModal(confirmComponent, {
            title: options.title || "", message: options.message || "",
            confirmText: options.confirmText || qsTr("Confirm"), cancelText: options.cancelText || qsTr("Cancel"),
            destructive: !!options.destructive })
        if (!d) return
        if (onConfirm) d.accepted.connect(onConfirm)
        if (onCancel) d.rejected.connect(onCancel)
    }
    function openMenu(options, onSelect) {
        var m = _createModal(menuComponent, { title: options.title || "", actions: options.actions || [] })
        if (m && onSelect) m.selected.connect(onSelect)
    }
    function openError(options, onAction) {
        var d = _createModal(messageComponent, {
            title: options.title || qsTr("Something went wrong"), message: options.message || "",
            details: options.details || "", kind: options.kind || "error",
            actions: options.actions || [{ id: "close", label: qsTr("Close"), primary: true }] })
        if (d && onAction) d.actionTriggered.connect(onAction)
    }
    function openKeyboard(options, onTextChanged, onDone) {
        var k = _createModal(keyboardComponent, { title: options.title || qsTr("Enter text"), text: options.text || "" })
        if (!k) return
        if (onTextChanged) k.textEdited.connect(onTextChanged)
        if (onDone) k.done.connect(onDone)
    }

    // ---- launching ------------------------------------------------------
    property string _pendingGameTitle: ""
    function launchGame(path) {
        if (Launcher.busy) { showToast(qsTr("Another application is already running"), "warning"); return }
        var game = Library.get(path)
        if (!game.path) { showToast(qsTr("This game is no longer in the library"), "error"); return }
        if (!game.emulatorId) {
            openError({ title: qsTr("No emulator assigned"),
                        message: qsTr("No configured emulator handles .%1 files. Choose an emulator for \"%2\" or add one to emulators.json.").arg(game.extension).arg(game.title),
                        actions: [{ id: "choose", label: qsTr("Choose emulator"), primary: true },
                                  { id: "close", label: qsTr("Cancel") }] },
                      function(id) { if (id === "choose") GameActions.chooseEmulator(path) })
            return
        }
        var emu = Emulators.byId(game.emulatorId)
        if (!emu.canLaunch) {
            _showUnavailable(emu)
            return
        }
        launchOverlay.gameTitle = game.title
        Launcher.launch(game.emulatorId, path)
    }
    function launchEmulator(id) {
        if (Launcher.busy) { showToast(qsTr("Another application is already running"), "warning"); return }
        var emu = Emulators.byId(id)
        if (!emu.id) return
        if (!emu.canLaunch) { _showUnavailable(emu); return }
        launchOverlay.gameTitle = ""
        Launcher.launch(id, "")
    }
    function _showUnavailable(emu) {
        openError({ title: qsTr("%1 can't be launched").arg(emu.name || qsTr("Emulator")),
                    message: qsTr("Status: %1").arg(emu.statusText || qsTr("Unknown")),
                    details: (emu.validationMessages || []).join("\n"),
                    actions: [{ id: "details", label: qsTr("View emulator"), primary: true },
                              { id: "close", label: qsTr("Close") }] },
                  function(id) { if (id === "details") { showSection("emulators", false); pushPage("EmulatorDetailsPage", { emulatorId: emu.id }) } })
    }

    Connections {
        target: Launcher
        function onLaunchFailed(emulatorId, gamePath, message) {
            shell.openError({ title: qsTr("Launch failed"), message: message,
                              details: qsTr("Log file: %1").arg(Diagnostics.logFilePath),
                              actions: [{ id: "close", label: qsTr("OK"), primary: true },
                                        { id: "logs", label: qsTr("View logs") }] },
                            function(id) { if (id === "logs") shell.openSettings("system") })
        }
        function onProcessFinished(emulatorId, gamePath, exitCode, result, durationMs) {
            if (result === Launcher.NormalExit) {
                shell.showToast(qsTr("Welcome back"), "info")
                return
            }
            var crashed = result === Launcher.Crashed
            var name = Emulators.byId(emulatorId).name || qsTr("The emulator")
            shell.openError({
                title: crashed ? qsTr("%1 stopped unexpectedly").arg(name)
                               : qsTr("%1 exited with an error").arg(name),
                message: Launcher.lastErrorMessage,
                details: (crashed ? qsTr("The process crashed.") : qsTr("Exit code: %1").arg(exitCode))
                         + "\n" + qsTr("Log file: %1").arg(Diagnostics.logFilePath),
                actions: [{ id: "retry", label: qsTr("Retry"), primary: true },
                          { id: "library", label: qsTr("Back to Library") },
                          { id: "logs", label: qsTr("View logs") }]
            }, function(id) {
                if (id === "retry") { if (!Launcher.retry()) shell.showToast(qsTr("Retry is not possible right now"), "warning") }
                else if (id === "library") shell.showSection("library", true)
                else if (id === "logs") shell.openSettings("system")
            })
        }
    }
    Connections {
        target: Controllers
        function onControllerConnected(name) { shell.showToast(qsTr("Controller connected: %1").arg(name), "success") }
        function onControllerDisconnected(name) { shell.showToast(qsTr("Controller disconnected: %1 — keyboard still works").arg(name), "warning") }
    }

    // ---- global keys (reached only when nothing deeper accepted the key) --
    Keys.onPressed: function(event) {
        if (Input.isBack(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) goBack()
        } else if (event.key === Qt.Key_Left && !sidebar.activeFocus) {
            event.accepted = true
            focusSidebar()
        }
    }

    // ---- layout -------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundPrimary
        // subtle top-right glow for depth
        Rectangle {
            width: parent.width * 0.7; height: width
            radius: width / 2
            x: parent.width - width * 0.55; y: -height * 0.6
            color: Theme.accentSecondary
            opacity: 0.025
        }
    }

    NexoraSidebar {
        id: sidebar
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: implicitWidth
        compact: shell.compactLayout
        currentSection: shell.currentSection
        onSectionRequested: function(section) { shell.showSection(section, false) }
        onEnterContent: shell.focusContent()
    }

    Item {
        id: contentArea
        anchors.top: parent.top
        anchors.bottom: hintBar.top
        anchors.left: sidebar.right
        anchors.right: parent.right

        Repeater {
            id: stacks
            model: Router.sections
            delegate: FocusScope {
                id: holder
                required property string modelData
                required property int index
                readonly property alias stack: stackView
                property bool loaded: false
                anchors.fill: parent
                visible: shell.currentSection === modelData
                onVisibleChanged: if (visible) shell.currentStack = stackView
                Component.onCompleted: if (visible) shell.currentStack = stackView
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Motion.page } }
                function ensureLoaded() {
                    if (loaded) return
                    loaded = true
                    stackView.push(Qt.resolvedUrl(shell.sectionPages[modelData] + ".qml"))
                }
                StackView {
                    id: stackView
                    anchors.fill: parent
                    focus: true
                    pushEnter: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.page }
                            NumberAnimation { property: "x"; from: Dimensions.s(40); to: 0; duration: Motion.page; easing.type: Motion.easing }
                        }
                    }
                    pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.page } }
                    popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.page } }
                    popExit: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.page }
                            NumberAnimation { property: "x"; from: 0; to: Dimensions.s(40); duration: Motion.page; easing.type: Motion.easing }
                        }
                    }
                    replaceEnter: pushEnter
                    replaceExit: pushExit
                }
            }
        }
    }

    ControllerHints {
        id: hintBar
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Dimensions.pagePaddingH
        anchors.rightMargin: Dimensions.pagePaddingH
        hints: {
            if (shell.sidebarFocused)
                return [{ action: "confirm", label: qsTr("Open") }].concat(
                    shell.currentSection !== "home" ? [{ action: "back", label: qsTr("Home") }] : [])
            var page = shell.currentPage
            var list = page && page.hints ? page.hints.slice() : []
            var hasBack = list.some(function(h) { return h.action === "back" })
            if (!hasBack) list.push({ action: "back", label: qsTr("Back") })
            return list
        }
    }

    LaunchOverlay { id: launchOverlay; z: 50 }

    Item {
        id: modalLayer
        anchors.fill: parent
        z: 100
    }

    ToastHost {
        id: toastHost
        z: 200
        anchors.right: parent.right
        anchors.bottom: hintBar.top
        anchors.margins: Dimensions.pagePaddingH
        width: Dimensions.s(520)
        height: parent.height / 2
    }

    Component { id: confirmComponent; ConfirmationDialog {} }
    Component { id: menuComponent; ActionMenu {} }
    Component { id: messageComponent; MessageDialog {} }
    Component { id: keyboardComponent; OnScreenKeyboard {} }
}
