pragma Singleton
import QtQuick

// Logical input actions. Controllers are translated to the same keys by the
// C++ ControllerManager, so these helpers cover keyboard and controller.
//   confirm   : Enter / Return / Space        (controller South)
//   back      : Esc / Backspace               (controller East)
//   secondary : F                             (controller West)
//   search    : /                             (controller North)
//   options   : M / Menu key                  (controller Start)
//   prevTab   : Page Up                       (controller LB)
//   nextTab   : Page Down                     (controller RB)
QtObject {
    function isConfirm(event) {
        return event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space
    }
    function isBack(event) {
        return event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace || event.key === Qt.Key_Back
    }
    function isSecondary(event) { return event.key === Qt.Key_F && !(event.modifiers & Qt.ControlModifier) }
    function isSearch(event) {
        return event.key === Qt.Key_Slash || (event.key === Qt.Key_F && (event.modifiers & Qt.ControlModifier))
    }
    function isOptions(event) { return event.key === Qt.Key_Menu || event.key === Qt.Key_M }
    function isPrevTab(event) { return event.key === Qt.Key_PageUp }
    function isNextTab(event) { return event.key === Qt.Key_PageDown }
}
