import QtQuick
import QtTest
import Nexora
import Nexora.Core

// Component-level behaviour: activation, auto-repeat protection, modal focus
// containment and restoration, toasts, on-screen keyboard.
Item {
    id: root
    width: 1280; height: 720

    FocusButton { id: button; x: 20; y: 20; text: "Press" }
    FocusButton { id: other; x: 20; y: 100; text: "Other" }
    SignalSpy { id: clickSpy; target: button; signalName: "clicked" }

    GameCard { id: card; x: 300; y: 20; title: "A Very Long Title That Must Be Elided Gracefully In The Card"; artworkUrl: "" }
    SignalSpy { id: activatedSpy; target: card; signalName: "activated" }
    SignalSpy { id: favoriteSpy; target: card; signalName: "favoriteToggled" }
    SignalSpy { id: optionsSpy; target: card; signalName: "optionsRequested" }

    ToastHost { id: toasts; anchors.fill: parent }

    Component { id: confirmComponent; ConfirmationDialog {} }
    Component { id: keyboardComponent; OnScreenKeyboard {} }

    TestCase {
        name: "Components"
        when: windowShown

        function init() {
            clickSpy.clear(); activatedSpy.clear(); favoriteSpy.clear(); optionsSpy.clear()
            button._lastActivation = 0
        }

        function test_buttonConfirmActivatesOnce() {
            button.forceActiveFocus()
            keyClick(Qt.Key_Return)
            compare(clickSpy.count, 1)
            // Rapid double activation is debounced.
            button._lastActivation = 0
            clickSpy.clear()
            keyClick(Qt.Key_Space)
            keyClick(Qt.Key_Space)
            compare(clickSpy.count, 1)
        }

        function test_disabledButtonDoesNothing() {
            button.enabled = false
            button.forceActiveFocus()
            keyClick(Qt.Key_Return)
            compare(clickSpy.count, 0)
            button.enabled = true
        }

        function test_gameCardKeys() {
            card.forceActiveFocus()
            verify(card.focused)
            keyClick(Qt.Key_Return)
            keyClick(Qt.Key_F)
            keyClick(Qt.Key_M)
            compare(activatedSpy.count, 1)
            compare(favoriteSpy.count, 1)
            compare(optionsSpy.count, 1)
        }

        function test_gameCardLongTitleStaysInBounds() {
            // Title text is elided to the card width (layout never grows).
            compare(card.width, card.implicitWidth)
            verify(card.height <= card.implicitWidth * Dimensions.cardAspect + Dimensions.cardTitleHeight + 1)
        }

        function test_modalContainsAndRestoresFocus() {
            other.forceActiveFocus()
            var dialog = confirmComponent.createObject(root, { title: "Confirm?", message: "Body" })
            verify(dialog)
            tryVerify(function() { return !other.activeFocus })
            // Navigation keys must not escape to items behind the modal.
            keyClick(Qt.Key_Up); keyClick(Qt.Key_Left); keyClick(Qt.Key_Down)
            verify(!other.activeFocus && !button.activeFocus)
            var rejected = false
            dialog.rejected.connect(function() { rejected = true })
            keyClick(Qt.Key_Escape)
            verify(rejected)
            tryVerify(function() { return other.activeFocus }, 2000, "focus restored to the previous item")
        }

        function test_toastsAreMergedAndCapped() {
            toasts.show("Hello", "info")
            toasts.show("Hello", "info")
            toasts.show("One", "success")
            toasts.show("Two", "warning")
            toasts.show("Three", "error")
            // Max three visible at a time.
            compare(toasts.count, 3, "max three visible, duplicates merged")
        }

        function test_onScreenKeyboardTyping() {
            var typed = ""
            var osk = keyboardComponent.createObject(root, { title: "Search" })
            osk.textEdited.connect(function(t) { typed = t })
            tryVerify(function() { return osk.activeFocus || osk.initialFocusItem.activeFocus })
            wait(50)
            keyClick(Qt.Key_Down)      // from "1" to "q"
            keyClick(Qt.Key_Return)
            compare(typed, "q")
            keyClick(Qt.Key_X)         // physical keyboard typing goes straight in
            compare(typed, "qx")
            keyClick(Qt.Key_Backspace)
            compare(typed, "q")
            var done = ""
            osk.done.connect(function(t) { done = t })
            keyClick(Qt.Key_Menu)
            compare(done, "q")
        }
    }
}
