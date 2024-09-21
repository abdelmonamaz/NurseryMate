import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Stock (M03) : vue par variante, alertes seuil, mouvements
// entrée / sortie / transfert (F03-02, F03-03, F03-05).
Item {
    id: page

    Component.onCompleted: Stock.refresh()

    Connections {
        target: Stock

        function onMoveRecorded() {
            moveDialog.close();
            moveDialog.clearForm();
        }
        function onErrorOccurred(message) {
            if (moveDialog.opened)
                moveDialog.errorText = message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s3

        // ── Barre d'outils ────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: NTheme.s3

            NTextField {
                Layout.preferredWidth: 300
                placeholderText: qsTr("Rechercher un produit…")
                onTextEdited: Stock.searchTerm = text
            }

            NComboBox {
                Layout.preferredWidth: 220
                model: [{ id: 0, label: qsTr("Tous les emplacements") }]
                    .concat(Stock.locationOptions())
                textRole: "label"
                valueRole: "id"
                onActivated: Stock.locationFilter = currentValue
            }

            Item { Layout.fillWidth: true }

            NButton {
                text: qsTr("🕘 Historique")
                variant: "ghost"
                onClicked: historyDialog.openFor(0, qsTr("Tous les mouvements"))
            }
            // Trois actions équivalentes -> même poids visuel (secondary)
            NButton {
                text: qsTr("⬇ Entrée")
                variant: "secondary"
                onClicked: moveDialog.openFor("in")
            }
            NButton {
                text: qsTr("⬆ Sortie")
                variant: "secondary"
                onClicked: moveDialog.openFor("out")
            }
            NButton {
                text: qsTr("🔄 Transfert")
                variant: "secondary"
                onClicked: moveDialog.openFor("transfer")
            }
        }

        // ── Liste du stock ────────────────────────────────────
        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: stockList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Stock.stock
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: row

                    required property int index
                    required property int variantId
                    required property string productFr
                    required property string productAr
                    required property string packaging
                    required property string sku
                    required property int qty
                    required property int alertThreshold
                    required property bool isLow
                    required property bool isNegative

                    width: ListView.view.width
                    height: 60
                    showSeparator: index < stockList.count - 1
                    // Chaque chiffre peut s'expliquer (doc 03 §1.5)
                    onClicked: historyDialog.openFor(
                        row.variantId, row.productFr + " — " + row.packaging)

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

                    NBadge {
                        visible: row.isNegative
                        text: qsTr("Négatif")
                        badgeColor: NTheme.danger
                    }
                    NBadge {
                        visible: row.isLow && !row.isNegative
                        text: qsTr("Stock bas")
                        badgeColor: NTheme.warning
                    }

                    Text {
                        text: row.qty
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeKpi
                        font.weight: Font.Bold
                        color: row.isNegative ? NTheme.danger
                             : row.isLow ? NTheme.warning
                             : NTheme.primary
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: stockList.count === 0
                    emoji: "📦"
                    message: qsTr("Aucune variante — créez d'abord des produits au catalogue.")
                }
            }
        }
    }

    // ── Dialogue de mouvement (F03-03) ────────────────────────
    NDialog {
        id: moveDialog

        property string kind: "in"
        property int correctionOf: 0 // id du mouvement corrigé, 0 sinon

        function openFor(moveKind) {
            clearForm();
            kind = moveKind;
            open();
        }

        // Contre-mouvement guidé : pré-remplit l'inverse du mouvement
        // erroné (norme : on ne supprime jamais, on contre-passe).
        function openCorrection(m) {
            clearForm();
            correctionOf = m.id;
            kind = m.kindCode === "in" ? "out"
                 : m.kindCode === "out" ? "in" : "transfer";
            picker.preset({ variantId: m.variantId, label: m.label });
            qtyField.text = String(m.qty);
            // L'inverse : une entrée se corrige par une sortie DEPUIS
            // l'emplacement livré, une sortie par une entrée VERS
            // l'emplacement débité, un transfert en le renversant.
            if (kind === "out")
                fromBox.currentIndex = Math.max(0, fromBox.indexOfValue(m.toId));
            else if (kind === "in")
                toBox.currentIndex = Math.max(0, toBox.indexOfValue(m.fromId));
            else {
                fromBox.currentIndex = Math.max(0, fromBox.indexOfValue(m.toId));
                toBox.currentIndex = Math.max(0, toBox.indexOfValue(m.fromId));
            }
            noteField.text = qsTr("Correction du mouvement #%1").arg(m.id)
                + (m.reason.length > 0 ? " — " + m.reason : "");
            open();
        }

        function clearForm() {
            picker.reset();
            qtyField.text = "";
            noteField.text = "";
            lossBox.currentIndex = 0;
            errorText = "";
            correctionOf = 0;
        }

        // Transfert : deux combos d'emplacement + quantité côte à côte
        width: kind === "transfer" ? 560 : 440
        title: (correctionOf > 0 ? qsTr("↩ Correction — ") : "")
             + (kind === "in" ? qsTr("Entrée de stock")
             : kind === "out" ? qsTr("Sortie de stock")
             : qsTr("Transfert entre emplacements"))
        onCancelClicked: clearForm()
        onAcceptClicked: Stock.recordMove({
            kind: moveDialog.kind,
            variantId: picker.selectedVariantId,
            fromId: moveDialog.kind !== "in" ? fromBox.currentValue : 0,
            toId: moveDialog.kind !== "out" ? toBox.currentValue : 0,
            qty: parseInt(qtyField.text) || 0,
            lossReason: moveDialog.kind === "out" ? lossBox.currentValue : "",
            note: noteField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Produit *") }
            VariantPicker {
                id: picker
                Layout.fillWidth: true
                searchFunction: function (term) { return Stock.searchVariants(term); }
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    visible: moveDialog.kind !== "in"
                    Layout.fillWidth: true
                    spacing: NTheme.s1

                    NFieldLabel { text: qsTr("Depuis") }
                    NComboBox {
                        id: fromBox
                        Layout.fillWidth: true
                        Layout.preferredWidth: 180
                        model: Stock.locationOptions()
                        textRole: "label"
                        valueRole: "id"
                    }
                }
                ColumnLayout {
                    visible: moveDialog.kind !== "out"
                    Layout.fillWidth: true
                    spacing: NTheme.s1

                    NFieldLabel { text: qsTr("Vers") }
                    NComboBox {
                        id: toBox
                        Layout.fillWidth: true
                        Layout.preferredWidth: 180
                        model: Stock.locationOptions()
                        textRole: "label"
                        valueRole: "id"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1

                    NFieldLabel { text: qsTr("Quantité *") }
                    NTextField {
                        id: qtyField
                        Layout.preferredWidth: 100
                        fieldColor: NTheme.surface
                        placeholderText: "0"
                        validator: IntValidator { bottom: 1 }
                    }
                }
            }

            // Motif de perte typé pour une sortie (F03-08) — optionnel :
            // une sortie peut aussi être un usage interne, une correction…
            ColumnLayout {
                visible: moveDialog.kind === "out"
                spacing: NTheme.s1

                NFieldLabel { text: qsTr("Motif de perte (si c'en est une)") }
                NComboBox {
                    id: lossBox
                    Layout.preferredWidth: 220
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("— pas une perte —"), value: "" },
                        { label: qsTr("Mortalité"), value: "mortality" },
                        { label: qsTr("Maladie"), value: "disease" },
                        { label: qsTr("Gel"), value: "frost" },
                        { label: qsTr("Casse"), value: "breakage" },
                        { label: qsTr("Autre perte"), value: "other" },
                    ]
                }
            }

            NFieldLabel { text: qsTr("Note") }
            NTextField {
                id: noteField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("optionnel")
            }
        }
    }

    // ── Historique des mouvements (F03-07) ────────────────────
    NDialog {
        id: historyDialog

        property var rows: []

        function openFor(variantId, label) {
            rows = Stock.history(variantId);
            title = qsTr("Historique — %1").arg(label);
            open();
        }

        width: 620
        acceptText: qsTr("Fermer")
        cancelText: ""
        onAcceptClicked: close()

        contentItem: ListView {
            id: historyList

            implicitHeight: Math.min(count, 10) * 44 + 8
            clip: true
            model: historyDialog.rows
            boundsBehavior: Flickable.StopAtBounds

            delegate: NListRow {
                id: moveRow

                required property int index
                required property var modelData

                width: ListView.view.width
                height: 44
                hoverable: false
                showSeparator: index < historyList.count - 1

                Text {
                    text: moveRow.modelData.dateTime
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                NBadge {
                    text: moveRow.modelData.kind
                    badgeColor: moveRow.modelData.kindCode === "in" ? NTheme.leaf
                        : moveRow.modelData.kindCode === "out" ? NTheme.danger
                        : moveRow.modelData.kindCode === "transfer" ? NTheme.info
                        : NTheme.warning
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        text: moveRow.modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: moveRow.modelData.flow
                            + (moveRow.modelData.origin.length > 0
                               ? "  ·  " + moveRow.modelData.origin : "")
                            + (moveRow.modelData.userName.length > 0
                               ? "  ·  " + moveRow.modelData.userName : "")
                            + (moveRow.modelData.reason.length > 0
                               ? "  ·  " + moveRow.modelData.reason : "")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                }
                Text {
                    text: "× " + moveRow.modelData.qty
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
                // Correction guidée : pré-remplit le contre-mouvement
                // (mouvements saisis à la main uniquement)
                NIconButton {
                    visible: moveRow.modelData.manual
                    text: "↩"
                    implicitHeight: 30
                    onClicked: {
                        historyDialog.close();
                        moveDialog.openCorrection(moveRow.modelData);
                    }
                }
            }

            NEmptyState {
                anchors.centerIn: parent
                visible: historyList.count === 0
                emoji: "🕘"
                message: qsTr("Aucun mouvement.")
            }
        }
    }
}
