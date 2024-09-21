import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Inventaire terrain (F09) : comptage par emplacement, hors ligne.
// Les écarts partent en ajustements (kind=adjust) dans la file de sync —
// faits additifs idempotents, visibles dans l'historique du poste
// principal avec la note « inventaire terrain ».
Item {
    id: page

    property var lines: []       // {variantId, label, sku, expected, counted}
    property int countedCount: {
        var total = 0;
        for (var i = 0; i < lines.length; ++i)
            if (lines[i].counted >= 0)
                ++total;
        return total;
    }
    property int gapCount: {
        var total = 0;
        for (var i = 0; i < lines.length; ++i)
            if (lines[i].counted >= 0 && lines[i].counted !== lines[i].expected)
                ++total;
        return total;
    }

    function reload() {
        lines = Sync.inventoryLines(locationBox.currentValue)
            .map(function (line) { line.counted = -1; return line; });
    }

    function validate() {
        // Un ajustement par écart : + vers l'emplacement, − depuis.
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i];
            if (line.counted < 0 || line.counted === line.expected)
                continue;
            var gap = line.counted - line.expected;
            Sync.queueMove({
                kind: "adjust",
                variantId: line.variantId,
                fromId: gap < 0 ? locationBox.currentValue : 0,
                toId: gap > 0 ? locationBox.currentValue : 0,
                qty: Math.abs(gap),
                note: qsTr("inventaire terrain"),
            });
        }
        page.StackView.view.pop();
    }

    Component.onCompleted: reload()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s3
        spacing: NTheme.s2

        RowLayout {
            spacing: NTheme.s2
            NIconButton {
                text: "←"
                implicitHeight: 44
                onClicked: page.StackView.view.pop()
            }
            Text {
                text: qsTr("📋 Inventaire")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeTitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("%1/%2").arg(page.countedCount).arg(page.lines.length)
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSubtitle
                font.weight: Font.Bold
                color: NTheme.textSecondary
            }
        }

        NComboBox {
            id: locationBox
            Layout.fillWidth: true
            implicitHeight: NTheme.buttonHeightLarge
            textRole: "label"
            valueRole: "id"
            model: Sync.locationOptions()
            onActivated: page.reload()
        }

        ListView {
            id: lineList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: NTheme.s1
            model: page.lines
            boundsBehavior: Flickable.StopAtBounds

            delegate: NCard {
                id: row

                required property int index
                required property var modelData

                width: ListView.view.width
                height: 76
                border.color: row.modelData.counted < 0 ? NTheme.outline
                    : row.modelData.counted === row.modelData.expected
                      ? NTheme.leaf : NTheme.warning

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: NTheme.s3
                    anchors.rightMargin: NTheme.s3
                    spacing: NTheme.s2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: row.modelData.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            text: qsTr("théorique : %1").arg(row.modelData.expected)
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    NTextField {
                        Layout.preferredWidth: 76
                        implicitHeight: 52
                        fieldColor: NTheme.surface
                        horizontalAlignment: TextInput.AlignHCenter
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 0 }
                        text: row.modelData.counted >= 0
                            ? String(row.modelData.counted) : ""
                        placeholderText: "—"
                        onEditingFinished: {
                            if (text.length === 0)
                                return;
                            var updated = page.lines.slice();
                            updated[row.index].counted = parseInt(text);
                            page.lines = updated;
                        }
                    }
                    NButton {
                        text: "✓"
                        variant: row.modelData.counted === row.modelData.expected
                            ? "primary" : "secondary"
                        implicitWidth: 52
                        implicitHeight: 52
                        onClicked: {
                            var updated = page.lines.slice();
                            updated[row.index].counted =
                                updated[row.index].expected;
                            page.lines = updated;
                        }
                    }
                }
            }

            NEmptyState {
                anchors.centerIn: parent
                visible: lineList.count === 0
                emoji: "📋"
                message: qsTr("Aucun stock à cet emplacement — synchronisez d'abord.")
            }
        }

        NButton {
            Layout.fillWidth: true
            large: true
            enabled: page.countedCount > 0
            text: page.gapCount > 0
                ? qsTr("Valider — %n écart(s)", "", page.gapCount)
                : qsTr("Valider — aucun écart")
            onClicked: page.validate()
        }
    }
}
