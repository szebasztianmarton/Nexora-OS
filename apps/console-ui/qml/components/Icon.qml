import QtQuick
import QtQuick.Shapes

// Vector line icon drawn from built-in path data on a 24x24 grid. No external
// assets are required; colour follows `color`.
// Names: home library star star-filled gamepad settings activity info search refresh sort
//   filter play chevron-left chevron-right chevron-up chevron-down check close warning error
//   success folder folder-plus trash more monitor speaker keyboard list clock plus minus file
//   stop copy power chip menu terminal backspace
Item {
    id: icon

    property string name: "info"
    property color color: Theme.textPrimary
    property real size: Dimensions.iconSize
    property real strokeWidth: name === "more" ? 3.4 : 1.9

    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "home": "M3 11 L12 3.5 L21 11 M5.5 9.5 V20.5 H10 V15 H14 V20.5 H18.5 V9.5",
        "library": "M4 4 H10.5 V10.5 H4 Z M13.5 4 H20 V10.5 H13.5 Z M4 13.5 H10.5 V20 H4 Z M13.5 13.5 H20 V20 H13.5 Z",
        "star": "M12 3.2 L14.6 8.7 L20.6 9.4 L16.1 13.5 L17.3 19.5 L12 16.5 L6.7 19.5 L7.9 13.5 L3.4 9.4 L9.4 8.7 Z",
        "star-filled": "M12 3.2 L14.6 8.7 L20.6 9.4 L16.1 13.5 L17.3 19.5 L12 16.5 L6.7 19.5 L7.9 13.5 L3.4 9.4 L9.4 8.7 Z",
        "gamepad": "M7.5 7.5 H16.5 A4.5 4.5 0 0 1 21 12 V14.8 A2.9 2.9 0 0 1 15.9 16.7 L14.6 15.2 H9.4 L8.1 16.7 A2.9 2.9 0 0 1 3 14.8 V12 A4.5 4.5 0 0 1 7.5 7.5 Z M7.5 10.2 V13.6 M5.8 11.9 H9.2 M15.2 11.2 H15.3 M17.4 13.2 H17.5",
        "settings": "M4 6 H13.5 M17.5 6 H20 M15.5 4 V8 M4 12 H7.5 M11.5 12 H20 M9.5 10 V14 M4 18 H13.5 M17.5 18 H20 M15.5 16 V20",
        "activity": "M3 12.5 H7 L9.8 5.5 L14.2 18.5 L17 12.5 H21",
        "info": "M12 3 A9 9 0 1 1 11.99 3 Z M12 11 V16.5 M12 7.6 V7.7",
        "search": "M10.5 4 A6.5 6.5 0 1 1 10.49 4 Z M15.4 15.4 L20 20",
        "refresh": "M19.5 12 A7.5 7.5 0 1 1 17.3 6.7 M19.5 4 V8.4 H15.1",
        "sort": "M7 4 V20 M3.8 16.8 L7 20 L10.2 16.8 M17 20 V4 M13.8 7.2 L17 4 L20.2 7.2",
        "filter": "M3.5 5 H20.5 L14 12.8 V18.5 L10 20.5 V12.8 Z",
        "play": "M7.5 4.8 L18.5 12 L7.5 19.2 Z",
        "chevron-left": "M14.5 5 L7.5 12 L14.5 19",
        "chevron-right": "M9.5 5 L16.5 12 L9.5 19",
        "chevron-up": "M5 14.5 L12 7.5 L19 14.5",
        "chevron-down": "M5 9.5 L12 16.5 L19 9.5",
        "check": "M4.5 12.5 L9.5 17.5 L19.5 6.5",
        "close": "M6 6 L18 18 M18 6 L6 18",
        "warning": "M12 3.8 L21.2 19.8 H2.8 Z M12 10 V14.2 M12 17 V17.1",
        "error": "M12 3 A9 9 0 1 1 11.99 3 Z M9 9 L15 15 M15 9 L9 15",
        "success": "M12 3 A9 9 0 1 1 11.99 3 Z M8 12.3 L10.8 15 L16 9.3",
        "folder": "M3 6.5 A1.5 1.5 0 0 1 4.5 5 H9.5 L11.5 7 H19.5 A1.5 1.5 0 0 1 21 8.5 V17.5 A1.5 1.5 0 0 1 19.5 19 H4.5 A1.5 1.5 0 0 1 3 17.5 Z",
        "folder-plus": "M3 6.5 A1.5 1.5 0 0 1 4.5 5 H9.5 L11.5 7 H19.5 A1.5 1.5 0 0 1 21 8.5 V17.5 A1.5 1.5 0 0 1 19.5 19 H4.5 A1.5 1.5 0 0 1 3 17.5 Z M12 10 V16 M9 13 H15",
        "trash": "M4 7 H20 M9.5 7 V4.5 H14.5 V7 M6 7 L7 20 H17 L18 7 M10 11 V16 M14 11 V16",
        "more": "M5.5 12 H5.6 M12 12 H12.1 M18.5 12 H18.6",
        "monitor": "M3 4.5 H21 V16 H3 Z M8 20 H16 M12 16 V20",
        "speaker": "M4 9.5 H7.8 L12.5 5.5 V18.5 L7.8 14.5 H4 Z M15.8 9.2 A4 4 0 0 1 15.8 14.8 M18.4 6.6 A7.6 7.6 0 0 1 18.4 17.4",
        "keyboard": "M3 6 H21 V18 H3 Z M7 10 H7.1 M10.5 10 H10.6 M14 10 H14.1 M17.5 10 H17.6 M8 14.2 H16",
        "list": "M8.5 6 H20 M8.5 12 H20 M8.5 18 H20 M4.5 6 H4.6 M4.5 12 H4.6 M4.5 18 H4.6",
        "clock": "M12 3 A9 9 0 1 1 11.99 3 Z M12 7 V12 L15.5 14",
        "plus": "M12 5 V19 M5 12 H19",
        "minus": "M5 12 H19",
        "file": "M6 3 H14 L19 8 V21 H6 Z M14 3 V8 H19",
        "stop": "M7 7 H17 V17 H7 Z",
        "copy": "M8.5 8.5 H20 V20 H8.5 Z M15.5 8.5 V4 H4 V15.5 H8.5",
        "power": "M12 3 V11 M7 6.4 A7.8 7.8 0 1 0 17 6.4",
        "chip": "M7 7 H17 V17 H7 Z M10 3.5 V7 M14 3.5 V7 M10 17 V20.5 M14 17 V20.5 M3.5 10 H7 M3.5 14 H7 M17 10 H20.5 M17 14 H20.5",
        "menu": "M4 6.5 H20 M4 12 H20 M4 17.5 H20",
        "terminal": "M3.5 5 H20.5 V19 H3.5 Z M7.5 9.5 L10.5 12 L7.5 14.5 M12.5 15 H16.5",
        "backspace": "M8.5 5.5 H20.5 V18.5 H8.5 L3 12 Z M11.5 9.5 L16.5 14.5 M16.5 9.5 L11.5 14.5"
    })
    readonly property bool filled: name === "star-filled" || name === "play"

    Shape {
        width: 24
        height: 24
        scale: icon.size / 24
        transformOrigin: Item.TopLeft
        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.strokeWidth
            fillColor: icon.filled ? icon.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] || icon.paths["info"] }
        }
    }
}
