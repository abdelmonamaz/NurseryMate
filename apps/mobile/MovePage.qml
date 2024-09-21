import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Saisie terrain : transfert entre emplacements ou déclaration de perte
// (mode "transfer" | "loss"). Hors ligne : mis en file, envoyé à la sync.
Item {
    id: page

    property string mode: "transfer"
    property var picked: null

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
                text: page.mode === "loss" ? qsTr("⚠️ Déclarer une perte")
                                           : qsTr("🔄 Déplacer")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeTitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }
        }

        // ── Choix du plant ────────────────────────────────────
        NFieldLabel { text: qsTr("Plant / article *") }
        RowLayout {
            visible: page.picked !== null
            spacing: NTheme.s2
            NBadge {
                text: page.picked ? page.picked.label : ""
                badgeColor: NTheme.primary
            }
            NIconButton {
                text: "✕"
                implicitHeight: 36
                onClicked: { page.picked = null; pickField.text = ""; }
            }
            Item { Layout.fillWidth: true }
        }
        NTextField {
            id: pickField
            visible: page.picked === null
            Layout.fillWidth: true
            implicitHeight: NTheme.buttonHeightLarge
            fieldColor: NTheme.surfaceCard
            placeholderText: qsTr("Tapez 2 lettres ou scannez…")
            onTextEdited: pickList.model =
                text.length >= 2 ? Sync.searchVariants(text) : []
        }
        ListView {
            id: pickList

            visible: page.picked === null && count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count, 5) * 52
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: NListRow {
                id: pickRow

                required property var modelData

                width: ListView.view.width
                height: 52
                onClicked: {
                    page.picked = pickRow.modelData;
                    pickList.model = [];
                }

                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: pickRow.modelData.label
                        + "  ·  " + pickRow.modelData.sku
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    color: NTheme.textPrimary
                }
                Text {
                    text: pickRow.modelData.totalQty
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
            }
        }

        // ── Emplacements ──────────────────────────────────────
        NFieldLabel { text: qsTr("Depuis *") }
        NComboBox {
            id: fromBox
            Layout.fillWidth: true
            implicitHeight: NTheme.buttonHeightLarge
            textRole: "label"
            valueRole: "id"
            model: Sync.locationOptions()
        }
        ColumnLayout {
            visible: page.mode === "transfer"
            spacing: NTheme.s1
            NFieldLabel { text: qsTr("Vers *") }
            NComboBox {
                id: toBox
                Layout.fillWidth: true
                implicitHeight: NTheme.buttonHeightLarge
                textRole: "label"
                valueRole: "id"
                model: Sync.locationOptions()
            }
        }

        // ── Quantité (+/− tactiles) ───────────────────────────
        NFieldLabel { text: qsTr("Quantité *") }
        RowLayout {
            spacing: NTheme.s2
            NIconButton {
                text: "−"
                variant: "secondary"
                implicitHeight: NTheme.buttonHeightLarge
                onClicked: qtyField.text =
                    String(Math.max(1, (parseInt(qtyField.text) || 1) - 1))
            }
            NTextField {
                id: qtyField
                Layout.fillWidth: true
                implicitHeight: NTheme.buttonHeightLarge
                fieldColor: NTheme.surfaceCard
                horizontalAlignment: TextInput.AlignHCenter
                font.pixelSize: NTheme.fontSizeTitle
                text: "1"
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1 }
            }
            NIconButton {
                text: "＋"
                variant: "secondary"
                implicitHeight: NTheme.buttonHeightLarge
                onClicked: qtyField.text =
                    String((parseInt(qtyField.text) || 0) + 1)
            }
        }

        // ── Motif de perte (mode loss) ────────────────────────
        ColumnLayout {
            visible: page.mode === "loss"
            spacing: NTheme.s1
            NFieldLabel { text: qsTr("Motif *") }
            NComboBox {
                id: lossBox
                Layout.fillWidth: true
                implicitHeight: NTheme.buttonHeightLarge
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("Mortalité"), value: "mortality" },
                    { label: qsTr("Maladie"), value: "disease" },
                    { label: qsTr("Gel"), value: "frost" },
                    { label: qsTr("Casse"), value: "breakage" },
                    { label: qsTr("Autre"), value: "other" },
                ]
            }
        }

        Item { Layout.fillHeight: true }

        NButton {
            Layout.fillWidth: true
            large: true
            enabled: page.picked !== null && (parseInt(qtyField.text) || 0) > 0
                     && (page.mode !== "transfer"
                         || fromBox.currentValue !== toBox.currentValue)
            text: page.mode === "loss" ? qsTr("Déclarer la perte")
                                       : qsTr("Valider le déplacement")
            onClicked: Sync.queueMove({
                kind: page.mode === "loss" ? "out" : "transfer",
                variantId: page.picked.variantId,
                fromId: fromBox.currentValue,
                toId: page.mode === "transfer" ? toBox.currentValue : 0,
                qty: parseInt(qtyField.text),
                lossReason: page.mode === "loss" ? lossBox.currentValue : "",
                note: page.mode === "loss" ? qsTr("perte terrain") : "",
            })
        }
    }
}
