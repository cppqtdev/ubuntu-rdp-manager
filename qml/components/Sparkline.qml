import QtQuick
import UbuntuRdpManager

// Lightweight history chart. Repaints only when the data changes (every sample, ~2 s).
Canvas {
    id: root

    property var values: []
    property real maxValue: 100
    property color lineColor: Theme.accent
    property bool fill: true

    implicitHeight: 64
    antialiasing: true
    renderStrategy: Canvas.Cooperative

    onValuesChanged: requestPaint()
    onMaxValueChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const n = values ? values.length : 0
        // baseline grid
        ctx.strokeStyle = Theme.divider
        ctx.lineWidth = 1
        for (let g = 1; g < 4; ++g) {
            const gy = Math.round(height * g / 4) + 0.5
            ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
        }
        if (n < 2)
            return
        const maxV = Math.max(maxValue, 1e-9)
        const step = width / (n - 1)
        ctx.beginPath()
        for (let i = 0; i < n; ++i) {
            const x = i * step
            const y = height - Math.max(0, Math.min(1, values[i] / maxV)) * (height - 2) - 1
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.strokeStyle = lineColor
        ctx.lineWidth = 2
        ctx.stroke()
        if (fill) {
            ctx.lineTo(width, height)
            ctx.lineTo(0, height)
            ctx.closePath()
            ctx.fillStyle = Theme.tint(lineColor, 0.15)
            ctx.fill()
        }
    }
}
