pragma Singleton
import QtQuick
import Nexora.Core

// Shared game actions so every screen behaves identically (Home rows, the
// library grid, Favorites and the details page).
QtObject {
    function openDetails(path) {
        Router.push("GameDetailsPage", { gamePath: path })
    }

    function toggleFavorite(path) {
        var game = Library.get(path)
        if (!game.path)
            return
        var nowFavorite = Library.toggleFavorite(path)
        Router.toast(nowFavorite ? qsTr("Added to Favorites") : qsTr("Removed from Favorites"), "success")
    }

    function chooseEmulator(path, onChosen) {
        var game = Library.get(path)
        if (!game.path)
            return
        var actions = []
        for (var i = 0; i < Emulators.count; ++i) {
            var emu = Emulators.get(i)
            actions.push({ id: emu.id, label: emu.name + (emu.canLaunch ? "" : " — " + emu.statusText),
                           icon: emu.id === game.emulatorId ? "check" : "chip", enabled: emu.enabled })
        }
        if (game.emulatorOverridden)
            actions.push({ id: "__auto", label: qsTr("Use automatic selection"), icon: "refresh" })
        if (actions.length === 0) {
            Router.showError({ title: qsTr("No emulators configured"),
                               message: qsTr("Add an emulator to emulators.json, then reload the emulator list."),
                               actions: [{ id: "emulators", label: qsTr("Open Emulators"), primary: true }] },
                             function(id) { if (id === "emulators") Router.showSection("emulators") })
            return
        }
        Router.openMenu({ title: qsTr("Emulator for %1").arg(game.title), actions: actions }, function(id) {
            Library.setEmulatorOverride(path, id === "__auto" ? "" : id)
            Router.toast(qsTr("Emulator updated"), "success")
            if (onChosen) onChosen()
        })
    }

    function confirmRemove(path, onRemoved) {
        var game = Library.get(path)
        if (!game.path)
            return
        Router.confirm({
            title: qsTr("Remove from library?"),
            message: qsTr("\"%1\" will be hidden from Nexora. The game file stays on disk and can be restored from Settings › Game Library.").arg(game.title),
            confirmText: qsTr("Remove"),
            destructive: true
        }, function() {
            Library.removeFromLibrary(path)
            Router.toast(qsTr("Removed from library"), "info")
            if (onRemoved) onRemoved()
        })
    }

    function showFileInfo(path) {
        var game = Library.get(path)
        if (!game.path)
            return
        var lines = [qsTr("File: %1").arg(game.fileName),
                     qsTr("Folder: %1").arg(String(game.path).substring(0, String(game.path).length - String(game.fileName).length - 1)),
                     qsTr("Type: .%1").arg(game.extension),
                     qsTr("Size: %1").arg(formatSize(game.fileSize))]
        Router.showError({ title: game.title, message: lines.join("\n"), kind: "info",
                           actions: [{ id: "close", label: qsTr("Close"), primary: true }] })
    }

    // Context menu used by cards (Options button / right click).
    function openMenu(path, onRemoved) {
        var game = Library.get(path)
        if (!game.path)
            return
        Router.openMenu({
            title: game.title,
            actions: [
                { id: "launch", label: qsTr("Launch"), icon: "play", enabled: !Launcher.busy },
                { id: "details", label: qsTr("View details"), icon: "info" },
                { id: "favorite", label: game.favorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites"),
                  icon: game.favorite ? "star-filled" : "star" },
                { id: "emulator", label: qsTr("Change emulator"), icon: "chip" },
                { id: "file", label: qsTr("File information"), icon: "file" },
                { id: "remove", label: qsTr("Remove from library"), icon: "trash", destructive: true }
            ]
        }, function(id) {
            switch (id) {
            case "launch": Router.launchGame(path); break
            case "details": openDetails(path); break
            case "favorite": toggleFavorite(path); break
            case "emulator": chooseEmulator(path); break
            case "file": showFileInfo(path); break
            case "remove": confirmRemove(path, onRemoved); break
            }
        })
    }

    function formatSize(bytes) {
        var b = Number(bytes)
        if (!(b >= 0)) return qsTr("Unknown")
        var units = ["B", "KB", "MB", "GB", "TB"]
        var i = 0
        while (b >= 1024 && i < units.length - 1) { b /= 1024; ++i }
        return (i === 0 ? b : b.toFixed(1)) + " " + units[i]
    }

    // "Today", "Yesterday", "3 days ago" or a date. Returns "" for invalid dates.
    function formatRelative(date) {
        if (!date || isNaN(date.getTime())) return ""
        var now = new Date()
        var startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate())
        var days = Math.floor((startOfToday - new Date(date.getFullYear(), date.getMonth(), date.getDate())) / 86400000)
        if (days <= 0) return qsTr("Today")
        if (days === 1) return qsTr("Yesterday")
        if (days < 7) return qsTr("%n days ago", "", days)
        return Qt.formatDate(date, Qt.locale().dateFormat(Locale.ShortFormat))
    }
}
