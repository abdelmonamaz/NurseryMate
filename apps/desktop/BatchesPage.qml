import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Production (M08) : lots de culture, pertes typées, passage vendable.
Item {
    id: page

    Component.onCompleted: Production.refresh()

    Connections {
        target: Production
        function onBatchCreated(id) { createDialog.close(); createDialog.clearForm(); }
        function onEventRecorded() {
            lossDialog.close();
            sellableDialog.close();
            treatmentDialog.close();
            if (detailDialog.opened)
                detailDialog.reload();
            // Un passage en vendable crée une entrée de stock (F08)
            Stock.refresh();
        }
        function onErrorOccurred(message) {
            if (treatmentDialog.opened) treatmentDialog.errorText = message;
            else if (createDialog.opened) createDialog.errorText = message;
            else if (lossDialog.opened) lossDialog.errorText = message;
            else if (sellableDialog.opened) sellableDialog.errorText = message;
            else if (detailDialog.opened) detailDialog.errorText = message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s3

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Lots de production")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSubtitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }
            Item { Layout.fillWidth: true }
            NButton {
                text: qsTr("🌱 Nouveau lot")
                onClicked: createDialog.open()
            }
        }

        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: batchList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Production.batches
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: batchRow

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 72
                    showSeparator: index < batchList.count - 1
                    // Clic sur la ligne = détail + journal + corrections
                    onClicked: detailDialog.openFor(batchRow.modelData)

                    ColumnLayout {
                        Layout.preferredWidth: 260
                        spacing: 2

                        RowLayout {
                            spacing: NTheme.s2
                            Text {
                                text: batchRow.modelData.number
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                            NBadge {
                                text: batchRow.modelData.origin
                                badgeColor: NTheme.info
                            }
                        }
                        Text {
                            text: batchRow.modelData.productFr
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            text: batchRow.modelData.locationFr + " · " + batchRow.modelData.startedAt
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    // Stats : restant / vendables / pertes / survie
                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: qsTr("Restant : %1 / %2")
                                .arg(batchRow.modelData.qtyRemaining)
                                .arg(batchRow.modelData.qtyInitial)
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        RowLayout {
                            spacing: NTheme.s2
                            Text {
                                text: qsTr("✅ %1 vendables").arg(batchRow.modelData.qtySellable)
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.success
                            }
                            Text {
                                text: qsTr("💀 %1 pertes").arg(batchRow.modelData.qtyLost)
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.danger
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Taux de survie
                    ColumnLayout {
                        spacing: 0
                        NFieldLabel {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("SURVIE")
                        }
                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: batchRow.modelData.survivalPercent + " %"
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: batchRow.modelData.survivalPercent >= 80
                                ? NTheme.success
                                : batchRow.modelData.survivalPercent >= 50
                                  ? NTheme.warning : NTheme.danger
                        }
                    }

                    NButton {
                        text: qsTr("Perte")
                        variant: "secondary"
                        enabled: batchRow.modelData.qtyRemaining > 0
                        onClicked: lossDialog.openFor(batchRow.modelData)
                    }
                    NButton {
                        text: qsTr("→ Vendable")
                        enabled: batchRow.modelData.qtyRemaining > 0
                        onClicked: sellableDialog.openFor(batchRow.modelData)
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: batchList.count === 0
                    emoji: "🌱"
                    message: qsTr("Aucun lot en cours — démarrez un semis ou un bouturage.")
                    actionText: qsTr("🌱 Nouveau lot")
                    onActionClicked: createDialog.open()
                }
            }
        }
    }

    // ── Nouveau lot (F08-01) ──────────────────────────────────
    NDialog {
        id: createDialog

        function clearForm() {
            productBox.currentIndex = 0;
            originBox.currentIndex = 0;
            qtyField.text = "";
            notesField.text = "";
            errorText = "";
        }

        width: 460
        title: qsTr("Nouveau lot de production")
        acceptText: qsTr("Créer le lot")
        onOpened: qtyField.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Production.createBatch({
            productId: productBox.currentValue,
            origin: originBox.currentValue,
            qtyInitial: parseInt(qtyField.text) || 0,
            locationId: locationBox.currentValue,
            notes: notesField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Produit (plante) *") }
            NComboBox {
                id: productBox
                Layout.fillWidth: true
                model: Production.productOptions()
                textRole: "label"
                valueRole: "id"
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Origine") }
                    NComboBox {
                        id: originBox
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("Semis"), value: "seed" },
                            { label: qsTr("Bouturage"), value: "cutting" },
                            { label: qsTr("Division"), value: "division" },
                            { label: qsTr("Jeune plant acheté"), value: "young_plant" },
                        ]
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Quantité initiale *") }
                    NTextField {
                        id: qtyField
                        Layout.preferredWidth: 130
                        fieldColor: NTheme.surface
                        placeholderText: "500"
                        validator: IntValidator { bottom: 1 }
                    }
                }
            }

            NFieldLabel { text: qsTr("Emplacement") }
            NComboBox {
                id: locationBox
                Layout.fillWidth: true
                model: Production.locationOptions()
                textRole: "label"
                valueRole: "id"
            }

            NFieldLabel { text: qsTr("Notes") }
            NTextField {
                id: notesField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("optionnel")
            }
        }
    }

    // ── Déclarer une perte (F08-02) ───────────────────────────
    NDialog {
        id: lossDialog

        property var batch: null

        function openFor(b) {
            batch = b;
            lossQty.text = "";
            lossReason.currentIndex = 0;
            lossNote.text = "";
            errorText = "";
            open();
        }

        width: 420
        title: batch ? qsTr("Perte — %1 (%2 restants)")
                          .arg(batch.number).arg(batch.qtyRemaining) : ""
        acceptText: qsTr("Enregistrer la perte")
        onAcceptClicked: Production.recordLoss(
            batch.id, parseInt(lossQty.text) || 0,
            lossReason.currentValue, lossNote.text)

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Quantité perdue *") }
                    NTextField {
                        id: lossQty
                        Layout.preferredWidth: 110
                        fieldColor: NTheme.surface
                        validator: IntValidator { bottom: 1 }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Motif") }
                    NComboBox {
                        id: lossReason
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("💀 Mortalité"), value: "mortality" },
                            { label: qsTr("🦠 Maladie"), value: "disease" },
                            { label: qsTr("❄ Gel"), value: "frost" },
                            { label: qsTr("📦 Casse"), value: "breakage" },
                            { label: qsTr("Autre"), value: "other" },
                        ]
                    }
                }
            }

            NFieldLabel { text: qsTr("Note") }
            NTextField {
                id: lossNote
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("optionnel")
            }
        }
    }

    // ── Passage en vendable (RG-08.a) ─────────────────────────
    NDialog {
        id: sellableDialog

        property var batch: null

        function openFor(b) {
            batch = b;
            sellQty.text = "";
            sellVariant.model = Production.variantOptions(b.productId);
            errorText = "";
            open();
        }

        width: 440
        title: batch ? qsTr("Passer en vendable — %1 (%2 restants)")
                          .arg(batch.number).arg(batch.qtyRemaining) : ""
        acceptText: qsTr("Ajouter au stock")
        onAcceptClicked: Production.recordSellable(
            batch.id, parseInt(sellQty.text) || 0,
            sellVariant.currentValue, sellLocation.currentValue)

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Text {
                Layout.fillWidth: true
                text: qsTr("Les plants deviennent du stock commercial vendable.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Quantité *") }
                    NTextField {
                        id: sellQty
                        Layout.preferredWidth: 100
                        fieldColor: NTheme.surface
                        validator: IntValidator { bottom: 1 }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Conditionnement *") }
                    NComboBox {
                        id: sellVariant
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "id"
                    }
                }
            }

            NFieldLabel { text: qsTr("Emplacement de vente *") }
            NComboBox {
                id: sellLocation
                Layout.fillWidth: true
                model: Production.locationOptions()
                textRole: "label"
                valueRole: "id"
            }
        }
    }

    // ── Détail du lot : journal + corrections (norme) ─────────
    NDialog {
        id: detailDialog

        property var batch: null
        property var events: []
        property var treatments: []
        property bool deletable: false
        property bool confirmCancel: false
        property bool confirmDelete: false

        function openFor(batchData) {
            batch = batchData;
            reload();
            errorText = "";
            confirmCancel = false;
            confirmDelete = false;
            open();
        }
        function reload() {
            if (batch) {
                events = Production.batchEvents(batch.id);
                treatments = Production.batchTreatments(batch.id);
                deletable = Production.batchDeletable(batch.id);
            }
        }

        width: 560
        title: batch ? qsTr("Lot %1 — %2").arg(batch.number)
                           .arg(batch.productFr) : ""
        acceptText: qsTr("Fermer")
        cancelText: ""
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s2
                NBadge {
                    text: detailDialog.batch
                        ? qsTr("Restant %1 / %2")
                              .arg(detailDialog.batch.qtyRemaining)
                              .arg(detailDialog.batch.qtyInitial) : ""
                    badgeColor: NTheme.primary
                }
                NBadge {
                    text: detailDialog.batch
                        ? qsTr("✅ %1 vendables").arg(detailDialog.batch.qtySellable) : ""
                    badgeColor: NTheme.leaf
                }
                NBadge {
                    text: detailDialog.batch
                        ? qsTr("💀 %1 pertes").arg(detailDialog.batch.qtyLost) : ""
                    badgeColor: NTheme.danger
                }
                Item { Layout.fillWidth: true }
            }

            ListView {
                id: eventList

                Layout.fillWidth: true
                Layout.preferredHeight: count === 0
                    ? 90 : Math.min(count, 7) * 44 + 8
                clip: true
                model: detailDialog.events
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: eventRow

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 44
                    hoverable: false
                    showSeparator: index < eventList.count - 1
                    baseColor: eventRow.modelData.isCorrection
                        ? Qt.alpha(NTheme.warning, 0.07) : "transparent"

                    Text {
                        text: eventRow.modelData.dateTime
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: eventRow.modelData.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            color: NTheme.textPrimary
                        }
                        Text {
                            visible: eventRow.modelData.note.length > 0
                                     || eventRow.modelData.userName.length > 0
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: (eventRow.modelData.userName.length > 0
                                   ? eventRow.modelData.userName : "")
                                + (eventRow.modelData.note.length > 0
                                   ? "  ·  " + eventRow.modelData.note : "")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }
                    Text {
                        text: (eventRow.modelData.qty > 0 ? "× " : "+ ")
                            + Math.abs(eventRow.modelData.qty)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: eventRow.modelData.isCorrection
                            ? NTheme.warning : NTheme.textPrimary
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: eventList.count === 0
                    emoji: "🌱"
                    message: qsTr("Aucun événement — pertes et passages en vendable apparaîtront ici.")
                }
            }

            // ── Traitements & interventions (F08-03) ──────────
            RowLayout {
                Layout.fillWidth: true
                NFieldLabel { text: qsTr("Interventions") }
                Item { Layout.fillWidth: true }
                NButton {
                    text: qsTr("🧪 Intervention")
                    variant: "secondary"
                    onClicked: treatmentDialog.openFor(detailDialog.batch)
                }
            }
            ListView {
                id: treatmentList

                Layout.fillWidth: true
                Layout.preferredHeight: count === 0
                    ? 30 : Math.min(count, 4) * 38 + 4
                clip: true
                model: detailDialog.treatments
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: treatmentRow

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 38
                    hoverable: false
                    showSeparator: index < treatmentList.count - 1

                    Text {
                        text: treatmentRow.modelData.dateTime
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: treatmentRow.modelData.label
                            + (treatmentRow.modelData.note.length > 0
                               ? "  ·  " + treatmentRow.modelData.note : "")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: treatmentRow.modelData.userName
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: treatmentList.count === 0
                    text: qsTr("Aucune intervention — arrosage, fertilisation, phyto, taille…")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            RowLayout {
                spacing: NTheme.s2

                NButton {
                    visible: detailDialog.events.length > 0
                             && !detailDialog.events[0].isCorrection
                    text: detailDialog.confirmCancel
                        ? qsTr("⚠ Confirmer l'annulation ?")
                        : qsTr("↩ Annuler le dernier événement")
                    variant: "ghost"
                    onClicked: {
                        if (!detailDialog.confirmCancel) {
                            detailDialog.confirmCancel = true;
                        } else if (Production.cancelLastEvent(detailDialog.batch.id)) {
                            detailDialog.close();
                        }
                    }
                }
                NButton {
                    visible: detailDialog.deletable
                    text: detailDialog.confirmDelete
                        ? qsTr("⚠ Confirmer la suppression ?")
                        : qsTr("🗑 Supprimer (jamais utilisé)")
                    variant: "ghost"
                    onClicked: {
                        if (!detailDialog.confirmDelete) {
                            detailDialog.confirmDelete = true;
                        } else if (Production.deleteBatch(detailDialog.batch.id)) {
                            detailDialog.close();
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: detailDialog.events.length > 0
                Layout.fillWidth: true
                text: qsTr("Une annulation crée un contre-événement tracé : le restant est restauré et, pour un passage en vendable, le stock ressort.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }

    // ── Intervention sur un lot (F08-03) ──────────────────────
    NDialog {
        id: treatmentDialog

        property var batch: null

        function openFor(batchData) {
            batch = batchData;
            treatKindBox.currentIndex = 0;
            treatProductField.text = "";
            treatDoseField.text = "";
            treatNoteField.text = "";
            remindBox.currentIndex = 0;
            errorText = "";
            open();
        }

        width: 460
        title: batch ? qsTr("Intervention — %1").arg(batch.number) : ""
        acceptText: qsTr("Enregistrer")
        acceptEnabled: treatKindBox.currentValue !== "phyto"
                       || treatProductField.text.trim().length > 0
        onAcceptClicked: {
            // Rappel planifié dans la foulée (F08-04) : « Refaire :
            // fertilisation — L-2026-001 » dans N jours.
            if (Production.recordTreatment(
                    batch.id, treatKindBox.currentValue,
                    treatProductField.text, treatDoseField.text,
                    treatNoteField.text)
                && remindBox.currentValue > 0) {
                Reminders.addReminderInDays(
                    qsTr("Refaire : %1 — %2")
                        .arg(treatKindBox.currentText).arg(batch.number),
                    remindBox.currentValue, batch.id);
            }
        }

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Type *") }
            NComboBox {
                id: treatKindBox
                Layout.preferredWidth: 240
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("💧 Arrosage exceptionnel"), value: "watering" },
                    { label: qsTr("🌱 Fertilisation"), value: "fertilization" },
                    { label: qsTr("🧪 Traitement phyto"), value: "phyto" },
                    { label: qsTr("✂️ Taille"), value: "pruning" },
                    { label: qsTr("Autre intervention"), value: "other" },
                ]
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel {
                        text: treatKindBox.currentValue === "phyto"
                            ? qsTr("Produit utilisé *") : qsTr("Produit utilisé")
                    }
                    NTextField {
                        id: treatProductField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("ex. bouillie bordelaise")
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Dose") }
                    NTextField {
                        id: treatDoseField
                        Layout.preferredWidth: 120
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("ex. 20 g/L")
                    }
                }
            }

            NFieldLabel { text: qsTr("Note") }
            NTextField {
                id: treatNoteField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("optionnel")
            }

            NFieldLabel { text: qsTr("Me le rappeler (tableau de bord)") }
            NComboBox {
                id: remindBox
                Layout.preferredWidth: 200
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("— non —"), value: 0 },
                    { label: qsTr("Dans 7 jours"), value: 7 },
                    { label: qsTr("Dans 15 jours"), value: 15 },
                    { label: qsTr("Dans 30 jours"), value: 30 },
                ]
            }
        }
    }
}
