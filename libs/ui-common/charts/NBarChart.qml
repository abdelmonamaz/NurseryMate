import QtQuick
import QtQuick.Layouts

// Diagramme en barres (série unique, magnitude) : barres fines à extrémité
// arrondie ancrées à la ligne de base, label direct sur la plus haute,
// hover par barre. points : [{label, value, display, fullLabel}].
Item {
    id: chart

    property var points: []
    property color barColor: NTheme.leaf

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

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 16
        spacing: NTheme.s2

        Repeater {
            model: chart.points

            delegate: ColumnLayout {
                id: bar

                required property int index
                required property var modelData

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                // Valeur au-dessus de la plus haute barre (label direct)
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: bar.index === chart._maxIndex && bar.modelData.value > 0
                    text: bar.modelData.display
                    font.family: NTheme.fontFamily
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }

                Item { Layout.fillHeight: true; Layout.fillWidth: true
                    Rectangle {
                        width: Math.min(parent.width - 6, 34)
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        radius: 4
                        height: Math.max(2,
                            parent.height * bar.modelData.value / chart._maxValue)
                        color: barHover.hovered
                            ? NTheme.primary
                            : (bar.index === chart._maxIndex ? NTheme.primary : chart.barColor)

                        Behavior on height { NumberAnimation { duration: NTheme.animFast } }

                        HoverHandler { id: barHover }

                        // Tooltip
                        Rectangle {
                            visible: barHover.hovered
                            radius: NTheme.radiusButton
                            color: NTheme.textPrimary
                            width: tipText.width + NTheme.s2 * 2
                            height: tipText.height + NTheme.s1 * 2
                            anchors.bottom: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottomMargin: 4
                            z: 10

                            Text {
                                id: tipText
                                anchors.centerIn: parent
                                text: bar.modelData.fullLabel + " : " + bar.modelData.display
                                font.family: NTheme.fontFamily
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                color: NTheme.surfaceCard
                            }
                        }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: bar.modelData.label
                    font.family: NTheme.fontFamily
                    font.pixelSize: 9
                    color: NTheme.textSecondary
                }
            }
        }
    }
}
