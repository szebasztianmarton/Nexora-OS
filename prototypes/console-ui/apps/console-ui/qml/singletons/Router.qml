pragma Singleton
import QtQuick

// Application-level navigation and feedback API used by every page.
// The Shell registers itself as `shell` on startup; until then calls are no-ops.
//
//   Router.showSection("library")                 top-level sections: home, library, favorites, emulators, settings
//   Router.push("GameDetailsPage", { gamePath: p }) push a page file (without .qml) onto the current section
//   Router.back()                                  same as pressing Back; returns true if handled
//   Router.focusSidebar() / Router.focusContent()
//   Router.toast("Added to Favorites", "success")  kinds: info | success | warning | error
//   Router.confirm({ title, message, confirmText, cancelText, destructive }, onConfirm, onCancel)
//   Router.openMenu({ title, actions: [{ id, label, icon, enabled, destructive }] }, onSelect(id))
//   Router.showError({ title, message, details, actions: [{ id, label, primary }] }, onAction(id))
//   Router.launchGame(path) / Router.launchEmulator(id)   central launch with all feedback
//   Router.openSettings("system")            Settings section, specific category (general, display, audio,
//                                             controller, library, emulators, system, about)
//   Router.openKeyboard({ title, text }, onTextChanged(text), onDone(text))  on-screen keyboard
QtObject {
    property Item shell: null
    readonly property string currentSection: shell ? shell.currentSection : "home"
    readonly property var sections: ["home", "library", "favorites", "emulators", "settings"]

    function showSection(name, focusContent) { if (shell) shell.showSection(name, focusContent === undefined ? true : focusContent) }
    function push(page, properties) { if (shell) shell.pushPage(page, properties || {}) }
    function back() { return shell ? shell.goBack() : false }
    function focusSidebar() { if (shell) shell.focusSidebar() }
    function focusContent() { if (shell) shell.focusContent() }
    function toast(message, kind) { if (shell) shell.showToast(message, kind || "info") }
    function confirm(options, onConfirm, onCancel) { if (shell) shell.openConfirm(options, onConfirm, onCancel) }
    function openMenu(options, onSelect) { if (shell) shell.openMenu(options, onSelect) }
    function showError(options, onAction) { if (shell) shell.openError(options, onAction) }
    function launchGame(path) { if (shell) shell.launchGame(path) }
    function launchEmulator(id) { if (shell) shell.launchEmulator(id) }
    function openSettings(category) { if (shell) shell.openSettings(category) }
    function openKeyboard(options, onTextChanged, onDone) { if (shell) shell.openKeyboard(options, onTextChanged, onDone) }
}
