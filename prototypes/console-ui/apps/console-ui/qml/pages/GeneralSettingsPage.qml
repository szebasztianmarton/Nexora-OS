import QtQuick
import Nexora.Core

// Settings › General. Every change is saved immediately by AppSettings.
SettingsCategoryPage {
    id: root

    readonly property var scaleOptions: [0.8, 0.9, 1.0, 1.1, 1.2, 1.3].map(function(v) {
        return { value: v, label: Math.round(v * 100) + "%" }
    })
    readonly property var sortOptions: [
        { value: "title-asc", label: qsTr("A–Z") },
        { value: "title-desc", label: qsTr("Z–A") },
        { value: "recent", label: qsTr("Recently played") },
        { value: "added", label: qsTr("Recently added") }
    ]
    function scaleIndex() {
        var best = 0
        for (var i = 0; i < scaleOptions.length; ++i)
            if (Math.abs(scaleOptions[i].value - Settings.interfaceScale) < Math.abs(scaleOptions[best].value - Settings.interfaceScale))
                best = i
        return best
    }
    function sortIndex() {
        for (var i = 0; i < sortOptions.length; ++i)
            if (sortOptions[i].value === Settings.defaultSort) return i
        return 0
    }

    SettingsSection { title: qsTr("Language and appearance") }
    SettingsValueRow {
        label: qsTr("Language")
        description: qsTr("More languages will be added when translations exist.")
        value: qsTr("English")
    }
    SettingsValueRow {
        label: qsTr("Theme")
        description: qsTr("Nexora currently ships with a single dark theme.")
        value: qsTr("Dark")
    }
    SettingsCyclerRow {
        id: scaleRow
        label: qsTr("Interface scale")
        description: qsTr("Size of text and controls. Applies immediately.")
        options: root.scaleOptions
        currentIndex: root.scaleIndex()
        onSelected: function(index, value) { Settings.interfaceScale = value }
    }
    SettingsToggleRow {
        label: qsTr("Reduce motion")
        description: qsTr("Turns off animations. Focus stays clearly visible.")
        checked: Settings.reduceMotion
        onToggled: function(value) { Settings.reduceMotion = value }
    }
    SettingsToggleRow {
        label: qsTr("Compact sidebar")
        description: qsTr("Show only icons in the navigation sidebar.")
        checked: Settings.sidebarCompact
        onToggled: function(value) { Settings.sidebarCompact = value }
    }

    SettingsSection { title: qsTr("Library") }
    SettingsCyclerRow {
        label: qsTr("Default library sorting")
        description: qsTr("Order used when the Game Library opens.")
        options: root.sortOptions
        wrap: true
        currentIndex: root.sortIndex()
        onSelected: function(index, value) { Settings.defaultSort = value }
    }

    SettingsSection { title: qsTr("Reset") }
    SettingsActionRow { objectName: "resetRow"
        label: qsTr("Run first-time setup again")
        description: qsTr("Shows the welcome and setup steps again. Your folders and settings are kept.")
        buttonText: qsTr("Start setup")
        iconName: "refresh"
        onActivated: Router.confirm({
            title: qsTr("Run first-time setup again?"),
            message: qsTr("Nexora will return to the welcome screen. Your library folders and preferences are kept."),
            confirmText: qsTr("Start setup")
        }, function() { Settings.firstRunCompleted = false })
    }
    SettingsActionRow {
        label: qsTr("Restore default settings")
        description: qsTr("Resets interface scale, sorting, motion, sidebar and full-screen preferences. Library folders and games are not affected.")
        buttonText: qsTr("Restore")
        danger: true
        onActivated: Router.confirm({
            title: qsTr("Restore default settings?"),
            message: qsTr("Interface scale, sorting, motion, sidebar and full-screen preferences return to their defaults. Library folders, favorites and games are not affected."),
            confirmText: qsTr("Restore defaults"),
            destructive: true
        }, function() {
            Settings.restoreDefaults(false)
            Router.toast(qsTr("Default settings restored"), "success")
        })
    }
}
