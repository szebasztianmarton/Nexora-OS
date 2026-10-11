import QtQuick
import Nexora.Console

// Line icons for the game bar, drawn on a 24×24 grid so no icon font is needed on the image.
// Names: home, gear, speaker, chart, play, stop, grid, gamepad, wifi, ethernet, battery
// (uses |level| 0-100 and |charging|), power, reboot.
Canvas {
    id: icon
    property string name
    property color color: Theme.text
    property int level: 100
    property bool charging: false

    implicitWidth: 24
    implicitHeight: 24
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onLevelChanged: requestPaint()
    onChargingChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const s = Math.min(width, height) / 24
        ctx.translate((width - 24 * s) / 2, (height - 24 * s) / 2)
        ctx.scale(s, s)
        const c = String(icon.color)
        ctx.strokeStyle = c
        ctx.fillStyle = c
        ctx.lineWidth = 2
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        function line(points) {
            ctx.beginPath()
            ctx.moveTo(points[0], points[1])
            for (let i = 2; i < points.length; i += 2)
                ctx.lineTo(points[i], points[i + 1])
        }
        function roundRect(x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.arcTo(x + w, y, x + w, y + h, r)
            ctx.arcTo(x + w, y + h, x, y + h, r)
            ctx.arcTo(x, y + h, x, y, r)
            ctx.arcTo(x, y, x + w, y, r)
            ctx.closePath()
        }

        switch (icon.name) {
        case "home":
            line([3, 11, 12, 3, 21, 11]); ctx.stroke()
            line([5.5, 9, 5.5, 21, 10, 21, 10, 15, 14, 15, 14, 21, 18.5, 21, 18.5, 9]); ctx.stroke()
            break
        case "gear": {
            ctx.beginPath()
            for (let i = 0; i < 16; ++i) {
                const a = i * Math.PI / 8 - Math.PI / 16
                const r = (i % 2 === 0) ? 10 : 7.6
                const x = 12 + r * Math.cos(a), y = 12 + r * Math.sin(a)
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                const a2 = a + Math.PI / 8 * 0.55
                ctx.lineTo(12 + r * Math.cos(a2), 12 + r * Math.sin(a2))
            }
            ctx.closePath()
            ctx.fill()
            ctx.globalCompositeOperation = "destination-out"
            ctx.beginPath(); ctx.arc(12, 12, 3.4, 0, 2 * Math.PI); ctx.fill()
            ctx.globalCompositeOperation = "source-over"
            break
        }
        case "speaker":
            line([3, 9, 7, 9, 12, 4.5, 12, 19.5, 7, 15, 3, 15]); ctx.closePath(); ctx.fill()
            ctx.beginPath(); ctx.arc(13, 12, 4, -Math.PI / 4, Math.PI / 4); ctx.stroke()
            ctx.beginPath(); ctx.arc(13, 12, 8, -Math.PI / 4, Math.PI / 4); ctx.stroke()
            break
        case "chart":
            roundRect(2.5, 4, 19, 14, 2); ctx.stroke()
            line([5.5, 12, 8.5, 12, 10.5, 8, 13, 15, 15, 10, 16.5, 12, 18.5, 12]); ctx.stroke()
            line([9, 21.5, 15, 21.5]); ctx.stroke()
            line([12, 18, 12, 21.5]); ctx.stroke()
            break
        case "play":
            line([7, 4.5, 19.5, 12, 7, 19.5]); ctx.closePath(); ctx.fill()
            break
        case "stop":
            roundRect(5.5, 5.5, 13, 13, 2); ctx.fill()
            break
        case "grid":
            for (let gx = 0; gx < 2; ++gx)
                for (let gy = 0; gy < 2; ++gy) {
                    roundRect(3.5 + gx * 9.5, 3.5 + gy * 9.5, 7.5, 7.5, 1.5)
                    ctx.stroke()
                }
            break
        case "gamepad":
            ctx.beginPath()
            ctx.moveTo(7, 7)
            ctx.lineTo(17, 7)
            ctx.bezierCurveTo(21, 7, 23, 16, 21.5, 18.5)
            ctx.bezierCurveTo(20, 20.5, 17.5, 18, 16, 15.5)
            ctx.lineTo(8, 15.5)
            ctx.bezierCurveTo(6.5, 18, 4, 20.5, 2.5, 18.5)
            ctx.bezierCurveTo(1, 16, 3, 7, 7, 7)
            ctx.closePath()
            ctx.stroke()
            line([7.5, 9.5, 7.5, 13]); ctx.stroke()
            line([5.75, 11.25, 9.25, 11.25]); ctx.stroke()
            ctx.beginPath(); ctx.arc(16.5, 10, 1.1, 0, 2 * Math.PI); ctx.fill()
            ctx.beginPath(); ctx.arc(18.5, 12.5, 1.1, 0, 2 * Math.PI); ctx.fill()
            break
        case "wifi":
            for (let r = 0; r < 3; ++r) {
                ctx.beginPath()
                ctx.arc(12, 19, 4.5 + r * 4.5, -Math.PI * 0.78, -Math.PI * 0.22)
                ctx.stroke()
            }
            ctx.beginPath(); ctx.arc(12, 19, 1.6, 0, 2 * Math.PI); ctx.fill()
            break
        case "ethernet":
            roundRect(4, 4, 16, 14, 2); ctx.stroke()
            line([8, 18, 8, 21, 16, 21, 16, 18]); ctx.stroke()
            line([8.5, 8, 8.5, 11]); ctx.stroke()
            line([12, 8, 12, 11]); ctx.stroke()
            line([15.5, 8, 15.5, 11]); ctx.stroke()
            break
        case "battery": {
            roundRect(1.5, 6.5, 19, 11, 2.5); ctx.stroke()
            roundRect(21, 10, 2, 4, 1); ctx.fill()
            const fill = Math.max(0, Math.min(100, icon.level)) / 100 * 15
            if (fill > 0) {
                roundRect(3.5, 8.5, fill, 7, 1)
                ctx.fill()
            }
            if (icon.charging) {
                ctx.globalCompositeOperation = "xor"
                line([12.5, 7, 9, 12.5, 12, 12.5, 10.5, 17, 14.5, 11, 11.5, 11]); ctx.closePath(); ctx.fill()
                ctx.globalCompositeOperation = "source-over"
            }
            break
        }
        case "power":
            ctx.beginPath(); ctx.arc(12, 13, 8, -Math.PI * 0.3, Math.PI * 1.3); ctx.stroke()
            line([12, 3, 12, 12]); ctx.stroke()
            break
        case "reboot":
            ctx.beginPath(); ctx.arc(12, 12, 8, -Math.PI * 0.35, Math.PI * 1.5); ctx.stroke()
            line([12, 1.5, 12, 6.5, 17, 4]); ctx.closePath(); ctx.fill()
            break
        }
    }
}
