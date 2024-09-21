import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Inventaire guidé par emplacement (F03-06, doc 03 §4.5) :
// théorique figé -> comptage ligne à ligne -> validation = ajustements.
Item {
    id: page

    // Reprise automatique d'un comptage interrompu
    Component.onCompleted: Inventory.resumeAny()

    Connections {
        target: Inventory

        function onValidated(adjustments) {
            resultBanner.text = adjustments > 0
                ? qsTr("✅ Inventaire validé — %n ajustement(s) créé(s).", "", adjustments)
                : qsTr("✅ Inventaire validé — aucun écart.");
            resultBanner.visible = true;
            Stock.refresh();
        }
        function onErrorOccurred(message) {
            resultBanner.text = "⚠ " + message;
            resultBanner.visible = true;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s3

        // ── Démarrage / état ──────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: NTheme.s3

            NComboBox {
                id: locationBox

                Layout.preferredWidth: 220
                visible: !Inventory.active
                model: Inventory.locationOptions()
                textRole: "label"
                valueRole: "id"
            }

            NButton {
                visible: !Inventory.active
                text: qsTr("Démarrer / Reprendre")
                onClicked: {
                    resultBanner.visible = false;
                    Inventory.start(locationBox.currentValue);
                }
            }

            NBadge {
                visible: Inventory.active
                text: qsTr("Inventaire : %1").arg(Inventory.locationLabel)
                badgeColor: NTheme.primary
            }

            Text {
                visible: Inventory.active
                text: qsTr("%1/%2 comptés").arg(Inventory.countedCount)
                                           .arg(Inventory.totalCount)
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                font.weight: Font.DemiBold
                color: NTheme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: Inventory.active && Inventory.gapCount > 0
                text: qsTr("%n écart(s)", "", Inventory.gapCount)
                    + " (" + (Inventory.gapTotal > 0 ? "+" : "") + Inventory.gapTotal + ")"
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                font.weight: Font.Bold
                color: NTheme.warning
            }

            NButton {
                visible: Inventory.active
                text: qsTr("Abandonner")
                variant: "ghost"
                onClicked: Inventory.cancel()
            }
            NButton {
                visible: Inventory.active
                text: qsTr("Valider l'inventaire")
                enabled: Inventory.countedCount > 0
                onClicked: confirmDialog.open()
            }
        }

        // Bannière de résultat
        NCard {
            id: resultBanner

            property alias text: bannerText.text

            Layout.fillWidth: true
            Layout.preferredHeight: 44
            visible: false
            color: NTheme.primaryContainer
            border.color: NTheme.leaf

            Text {
                id: bannerText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: NTheme.s3
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textPrimary
            }
        }

        // ── Lignes de comptage ────────────────────────────────
        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: lineList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Inventory.lines
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: row

                    required property int index
                    required property int variantId
                    required property string productFr
                    required property string productAr
                    required property string packaging
                    required property string sku
                    required property int qtyExpected
                    required property int qtyCounted
                    required property bool isCounted
                    required property int gap

                    width: ListView.view.width
                    hoverable: false
                    showSeparator: index < lineList.count - 1
                    baseColor: row.isCounted && row.gap !== 0
                        ? Qt.alpha(NTheme.warning, 0.08)
                        : row.isCounted ? Qt.alpha(NTheme.leaf, 0.07)
                        : "transparent"

                    ColumnLayout {
                        spacing: 2

                        RowLayout {
                            spacing: NTheme.s2

                            Text {
                                text: row.productFr
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.DemiBold
                                color: NTheme.textPrimary
                            }
                            Text {
                                visible: row.productAr.length > 0
                                text: row.productAr
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textSecondary
                            }
                            NBadge { text: row.packaging; badgeColor: NTheme.info }
                        }
                        Text {
                            text: row.sku
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: qsTr("Théorique : %1").arg(row.qtyExpected)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textSecondary
                    }

                    NTextField {
                        id: countField

                        Layout.preferredWidth: 90
                        fieldColor: NTheme.surface
                        text: row.isCounted ? String(row.qtyCounted) : ""
                        placeholderText: qsTr("Compté")
                        horizontalAlignment: TextInput.AlignHCenter
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.DemiBold
                        validator: IntValidator { bottom: 0 }
                        onEditingFinished: {
                            if (text.length > 0)
                                Inventory.setCounted(row.variantId, parseInt(text));
                        }
                    }

                    // 1 tap : compté = théorique (doc 03 §4.5)
                    NButton {
                        text: qsTr("Identique ✓")
                        variant: "secondary"
                        enabled: !row.isCounted || row.qtyCounted !== row.qtyExpected
                        onClicked: Inventory.confirmExpected(row.variantId,
                                                             row.qtyExpected)
                    }

                    Text {
                        Layout.preferredWidth: 52
                        visible: row.isCounted
                        text: row.gap === 0 ? "✓"
                            : (row.gap > 0 ? "+" + row.gap : String(row.gap))
                        horizontalAlignment: Text.AlignRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: row.gap === 0 ? NTheme.success : NTheme.warning
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: lineList.count === 0
                    emoji: "📋"
                    message: Inventory.active
                        ? qsTr("Aucune variante en stock à cet emplacement.")
                        : qsTr("Choisissez un emplacement puis démarrez l'inventaire.")
                }
            }
        }
    }

    // Confirmation — la validation est irréversible (RG-03.c)
    NDialog {
        id: confirmDialog

        width: 400
        title: qsTr("Valider l'inventaire ?")
        acceptText: qsTr("Valider")
        onAcceptClicked: {
            confirmDialog.close();
            Inventory.validate();
        }

        contentItem: Text {
            text: qsTr("%1 ligne(s) comptée(s), %2 écart(s).\n"
                       + "Les écarts créeront des ajustements de stock.\n"
                       + "Cette action est irréversible.")
                .arg(Inventory.countedCount).arg(Inventory.gapCount)
            wrapMode: Text.WordWrap
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            color: NTheme.textPrimary
        }
    }
}
