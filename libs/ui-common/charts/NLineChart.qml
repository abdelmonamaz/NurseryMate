import QtQuick

// Courbe d'évolution (série unique) : aire + ligne, ligne de base à 0,
// grille discrète, label direct sur le point le plus haut, hover crosshair.
// points : [{label, value (nombre), display, fullLabel}].
// Design : une seule teinte (pas de souci CVD), marques fines, encre en
// jetons de texte (guide dataviz).
Item {
    id: chart

    property var points: []
    property color lineColor: NTheme.primary
    property real padL: 8
    property real padR: 12
    property real padT: 14
    property real padB: 20

    readonly property real _maxValue: {
        var m = 0;
        for (var i = 0; i < points.length; ++i)
            m = Math.max(m, points[i].value);
        return m > 0 ? m : 1;
    }
    readonly property int _maxIndex: {
        var m = -1, mi = 0;
        for (var i = 0; i < points.length; ++i)
            if (points[i].value > m) { m = points[i].value; mi = i; }
        return mi;
    }

    function _x(i) {
        if (points.length <= 1)
            return padL;
        return padL + (width - padL - padR) * i / (points.length - 1);
    }
    function _y(v) {
        return padT + (height - padT - padB) * (1 - v / _maxValue);
    }

    onPointsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        // Rendu sur le thread GUI : le thread de rendu ne voit pas les
        // polices applicatives (Inter) -> « font families … invalid ».
        renderStrategy: Canvas.Immediate

        // Context2D exige la famille entre guillemets.
        readonly property string fontSmall: '9px "' + NTheme.fontFamily + '"'
        readonly property string fontLabel: 'bold 10px "' + NTheme.fontFamily + '"'

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (chart.points.length === 0)
                return;

            var baseY = chart.height - chart.padB;

            // Grille horizontale discrète (3 lignes)
            ctx.strokeStyle = NTheme.outline;
            ctx.lineWidth = 1;
            ctx.font = canvas.fontSmall;
            ctx.fillStyle = NTheme.textSecondary;
            for (var g = 0; g <= 2; ++g) {
                var gv = chart._maxValue * g / 2;
                var gy = chart._y(gv);
                ctx.globalAlpha = 0.5;
                ctx.beginPath();
                ctx.moveTo(chart.padL, gy);
                ctx.lineTo(chart.width - chart.padR, gy);
                ctx.stroke();
                ctx.globalAlpha = 1;
            }

            // Aire sous la courbe
            ctx.beginPath();
            ctx.moveTo(chart._x(0), baseY);
            for (var i = 0; i < chart.points.length; ++i)
                ctx.lineTo(chart._x(i), chart._y(chart.points[i].value));
            ctx.lineTo(chart._x(chart.points.length - 1), baseY);
            ctx.closePath();
            ctx.fillStyle = chart.lineColor;
            ctx.globalAlpha = 0.12;
            ctx.fill();
            ctx.globalAlpha = 1;

            // Ligne (2px)
            ctx.beginPath();
            for (var j = 0; j < chart.points.length; ++j) {
                var px = chart._x(j), py = chart._y(chart.points[j].value);
                if (j === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
            }
            ctx.strokeStyle = chart.lineColor;
            ctx.lineWidth = 2;
            ctx.lineJoin = "round";
            ctx.stroke();

            // Marqueur + label direct sur le point le plus haut
            var mi = chart._maxIndex;
            if (chart.points[mi].value > 0) {
                var mx = chart._x(mi), my = chart._y(chart.points[mi].value);
                ctx.beginPath();
                ctx.arc(mx, my, 4, 0, 2 * Math.PI);
                ctx.fillStyle = chart.lineColor;
                ctx.fill();
                ctx.strokeStyle = NTheme.surfaceCard;
                ctx.lineWidth = 2;
                ctx.stroke();

                ctx.font = canvas.fontLabel;
                ctx.fillStyle = NTheme.textPrimary;
                ctx.textAlign = mi > chart.points.length / 2 ? "right" : "left";
                var lx = mi > chart.points.length / 2 ? mx - 6 : mx + 6;
                ctx.fillText(chart.points[mi].display, lx, my - 6);
            }

            // Étiquettes x : premier, milieu, dernier
            ctx.font = canvas.fontSmall;
            ctx.fillStyle = NTheme.textSecondary;
            var idxs = [0, Math.floor((chart.points.length - 1) / 2),
                        chart.points.length - 1];
            for (var k = 0; k < idxs.length; ++k) {
                var ix = idxs[k];
                ctx.textAlign = k === 0 ? "left" : (k === 2 ? "right" : "center");
                ctx.fillText(chart.points[ix].label, chart._x(ix), chart.height - 6);
            }
        }
    }

    // Hover : crosshair vertical + valeur du jour survolé
    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true

        property int nearest: -1

        onPositionChanged: {
            if (chart.points.length === 0) return;
            var best = 0, bestD = 1e9;
            for (var i = 0; i < chart.points.length; ++i) {
                var d = Math.abs(chart._x(i) - mouseX);
                if (d < bestD) { bestD = d; best = i; }
            }
            nearest = best;
        }
        onExited: nearest = -1
    }

    Rectangle {
        visible: hover.nearest >= 0
        width: 1
        color: NTheme.outline
        x: hover.nearest >= 0 ? chart._x(hover.nearest) : 0
        y: chart.padT
        height: chart.height - chart.padT - chart.padB
    }

    Rectangle {
        id: tip
        visible: hover.nearest >= 0
        radius: NTheme.radiusButton
        color: NTheme.textPrimary
        width: tipCol.width + NTheme.s2 * 2
        height: tipCol.height + NTheme.s2
        x: Math.min(chart.width - width,
                    Math.max(0, (hover.nearest >= 0 ? chart._x(hover.nearest) : 0) - width / 2))
        y: 0

        Column {
            id: tipCol
            anchors.centerIn: parent
            spacing: 0
            Text {
                text: hover.nearest >= 0 ? chart.points[hover.nearest].fullLabel : ""
                font.family: NTheme.fontFamily
                font.pixelSize: 9
                color: NTheme.surfaceCard
                opacity: 0.7
            }
            Text {
                text: hover.nearest >= 0 ? chart.points[hover.nearest].display : ""
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                font.weight: Font.Bold
                color: NTheme.surfaceCard
            }
        }
    }
}
