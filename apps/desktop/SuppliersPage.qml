import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Achats (M07) : fournisseurs (F07-01) + réceptions directes de
// marchandise (F07-04) — entrées de stock + coût moyen pondéré (F03-09).
Item {
    id: page

    Component.onCompleted: Suppliers.refresh()

    Connections {
        target: Suppliers

        function onSupplierCreated(supplierId) {
            addDialog.close();
            addDialog.clearForm();
        }
        function onReceiptRecorded() {
            receiptDialog.close();
            receiptDialog.clearForm();
            Stock.refresh();
        }
        function onOrderCreated(poId) {
            orderDialog.close();
            orderDialog.clearForm();
        }
        function onOrderReceived() {
            poReceiveDialog.close();
            Stock.refresh();
        }
        function onSupplierUpdated() {
            editDialog.close();
        }
        function onSupplierPaid() {
            paySupplierDialog.close();
        }
        function onErrorOccurred(message) {
            if (addDialog.opened)
                addDialog.errorText = message;
            else if (paySupplierDialog.opened)
                paySupplierDialog.errorText = message;
            else if (editDialog.opened)
                editDialog.errorText = message;
            else if (receiptDialog.opened)
                receiptDialog.errorText = message;
            else if (orderDialog.opened)
                orderDialog.errorText = message;
            else if (poReceiveDialog.opened)
                poReceiveDialog.errorText = message;
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s4

        // ══ Fournisseurs ══════════════════════════════════════
        ColumnLayout {
            Layout.preferredWidth: 380
            Layout.minimumWidth: 340
            Layout.fillHeight: true
            spacing: NTheme.s3

            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2

                NTextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Rechercher un fournisseur…")
                    onTextEdited: Suppliers.searchTerm = text
                }
                NButton {
                    text: Suppliers.showInactive ? "👁" : qsTr("Inactifs")
                    variant: "ghost"
                    onClicked: Suppliers.showInactive = !Suppliers.showInactive
                }
                NButton {
                    text: qsTr("＋")
                    implicitWidth: 40
                    onClicked: addDialog.open()
                }
            }

            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: supplierList

                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    model: Suppliers.suppliers
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: NListRow {
                        id: supplierRow

                        required property int index
                        required property var modelData

                        width: ListView.view.width
                        height: 56
                        showSeparator: index < supplierList.count - 1
                        // Clic = fiche fournisseur éditable (F07-01)
                        onClicked: editDialog.openFor(supplierRow.modelData)

                        Text { text: "🚚"; font.pixelSize: 20 }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: supplierRow.modelData.name
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: NTheme.textPrimary
                            }
                            Text {
                                visible: supplierRow.modelData.phone.length > 0
                                       || supplierRow.modelData.supplies.length > 0
                                text: [supplierRow.modelData.phone,
                                       supplierRow.modelData.supplies]
                                    .filter(function (s) { return s.length > 0; })
                                    .join(" · ")
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.textSecondary
                            }
                        }

                        NBadge {
                            visible: !supplierRow.modelData.active
                            text: qsTr("Inactif")
                            badgeColor: NTheme.danger
                        }

                        // Dette fournisseur (F07-05)
                        ColumnLayout {
                            spacing: 0
                            visible: supplierRow.modelData.hasDebt
                            NFieldLabel {
                                Layout.alignment: Qt.AlignRight
                                text: qsTr("DETTE")
                            }
                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: supplierRow.modelData.balanceDisplay
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.danger
                            }
                        }
                        NButton {
                            visible: supplierRow.modelData.hasDebt
                            text: qsTr("💰")
                            variant: "secondary"
                            implicitWidth: 52
                            onClicked: paySupplierDialog.openFor(supplierRow.modelData)
                        }
                    }

                    NEmptyState {
                        anchors.centerIn: parent
                        visible: supplierList.count === 0
                        emoji: "🚚"
                        message: qsTr("Aucun fournisseur.")
                        actionText: qsTr("＋ Ajouter")
                        onActionClicked: addDialog.open()
                    }
                }
            }
        }

        // ══ Commandes & réceptions (bascule) ══════════════════
        ColumnLayout {
            id: rightPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: NTheme.s3

            // 0 = Commandes d'achat, 1 = Réceptions directes
            property int tab: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2

                NButton {
                    text: qsTr("Commandes")
                    variant: rightPanel.tab === 0 ? "primary" : "ghost"
                    onClicked: rightPanel.tab = 0
                }
                NButton {
                    text: qsTr("Réceptions")
                    variant: rightPanel.tab === 1 ? "primary" : "ghost"
                    onClicked: rightPanel.tab = 1
                }
                Item { Layout.fillWidth: true }
                NButton {
                    text: qsTr("📅")
                    variant: "ghost"
                    implicitWidth: 52
                    onClicked: dueDialog.openNew()
                }
                NButton {
                    text: qsTr("🔍 Offres")
                    variant: "secondary"
                    onClicked: offerDialog.openNew()
                }
                NButton {
                    text: rightPanel.tab === 0 ? qsTr("🧾 Commander")
                                               : qsTr("📦 Réceptionner")
                    onClicked: rightPanel.tab === 0 ? orderDialog.openNew()
                                                    : receiptDialog.openNew()
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: rightPanel.tab

                // ── Commandes d'achat (F07-02) ────────────────
                NCard {
                    ListView {
                        id: orderList

                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: Suppliers.orders
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: orderRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 64
                            showSeparator: index < orderList.count - 1

                            ColumnLayout {
                                Layout.preferredWidth: 200
                                spacing: 2

                                RowLayout {
                                    spacing: NTheme.s2
                                    Text {
                                        text: orderRow.modelData.number
                                        font.family: NTheme.fontFamily
                                        font.pixelSize: NTheme.fontSizeBody
                                        font.weight: Font.Bold
                                        color: NTheme.primary
                                    }
                                    NBadge {
                                        text: orderRow.modelData.statusLabel
                                        badgeColor: orderRow.modelData.status === "received" ? NTheme.success
                                            : orderRow.modelData.status === "cancelled" ? NTheme.danger
                                            : orderRow.modelData.status === "partial" ? NTheme.warning
                                            : orderRow.modelData.status === "sent" ? NTheme.info
                                            : NTheme.textSecondary
                                    }
                                }
                                Text {
                                    text: orderRow.modelData.supplierName
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    text: qsTr("%1 reçus / %2 commandés")
                                        .arg(orderRow.modelData.qtyReceived)
                                        .arg(orderRow.modelData.qtyOrdered)
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                                Text {
                                    text: orderRow.modelData.date
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                            }

                            Text {
                                text: orderRow.modelData.totalCost
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                            NButton {
                                text: qsTr("Envoyer")
                                variant: "secondary"
                                visible: orderRow.modelData.status === "draft"
                                onClicked: Suppliers.sendOrder(orderRow.modelData.id)
                            }
                            NButton {
                                text: qsTr("↩")
                                variant: "ghost"
                                implicitWidth: 34
                                visible: (orderRow.modelData.status === "sent"
                                          || orderRow.modelData.status === "cancelled")
                                         && orderRow.modelData.qtyReceived === 0
                                onClicked: Suppliers.reopenOrder(orderRow.modelData.id)
                            }
                            NButton {
                                text: qsTr("Recevoir")
                                visible: orderRow.modelData.status === "sent"
                                         || orderRow.modelData.status === "partial"
                                onClicked: poReceiveDialog.openFor(orderRow.modelData.id)
                            }
                            NIconButton {
                                text: "✕"
                                implicitHeight: 30
                                visible: orderRow.modelData.status === "draft"
                                         || orderRow.modelData.status === "sent"
                                onClicked: Suppliers.cancelOrder(orderRow.modelData.id)
                            }
                        }

                        NEmptyState {
                            anchors.centerIn: parent
                            visible: orderList.count === 0
                            emoji: "🧾"
                            message: qsTr("Aucune commande — passez commande à un fournisseur.")
                            actionText: qsTr("🧾 Nouvelle commande")
                            onActionClicked: orderDialog.openNew()
                        }
                    }
                }

                // ── Réceptions directes (F07-04) ──────────────
                NCard {
                    ListView {
                        id: receiptList

                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: Suppliers.receipts
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: receiptRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 56
                            hoverable: false
                            showSeparator: index < receiptList.count - 1

                            Text {
                                text: receiptRow.modelData.dateTime
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.textSecondary
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: receiptRow.modelData.supplierName
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    font.weight: Font.DemiBold
                                    color: NTheme.textPrimary
                                }
                                Text {
                                    text: qsTr("%1 · %2 ligne(s) · %3 plants")
                                        .arg(receiptRow.modelData.locationFr)
                                        .arg(receiptRow.modelData.lineCount)
                                        .arg(receiptRow.modelData.totalQty)
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                            }
                            Text {
                                text: receiptRow.modelData.totalCost
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                        }

                        NEmptyState {
                            anchors.centerIn: parent
                            visible: receiptList.count === 0
                            emoji: "📦"
                            message: qsTr("Aucune réception — enregistrez vos achats de plants ici.")
                        }
                    }
                }
            }
        }
    }

    // ── Nouveau fournisseur (F07-01) ──────────────────────────
    NDialog {
        id: addDialog

        function clearForm() {
            nameField.text = "";
            phoneField.text = "";
            taxField.text = "";
            errorText = "";
        }

        width: 420
        title: qsTr("Nouveau fournisseur")
        acceptText: qsTr("Enregistrer")
        onOpened: nameField.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Suppliers.createSupplier({
            name: nameField.text,
            phone: phoneField.text,
            taxId: taxField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Raison sociale *") }
            NTextField {
                id: nameField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. Pépinières du Cap Bon")
            }
            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Téléphone") }
                    NTextField {
                        id: phoneField
                        Layout.preferredWidth: 170
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Matricule fiscal") }
                    NTextField {
                        id: taxField
                        Layout.preferredWidth: 160
                        fieldColor: NTheme.surface
                    }
                }
            }
        }
    }

    // ── Fiche fournisseur éditable (F07-01 complet) ───────────
    NDialog {
        id: editDialog

        property int supplierId: 0
        property bool supplierActive: true
        property bool deletable: false
        property bool confirmDelete: false
        property var products: []

        function reloadProducts() {
            products = Suppliers.supplierProducts(supplierId);
        }

        function openFor(s) {
            supplierId = s.id;
            supplierActive = s.active;
            deletable = Suppliers.isDeletable(s.id);
            confirmDelete = false;
            spPicker.reset();
            reloadProducts();
            eNameField.text = s.name;
            ePhoneField.text = s.phone;
            eEmailField.text = s.email;
            eAddressField.text = s.address;
            eTaxField.text = s.taxId;
            eTermsField.text = s.paymentTerms;
            eSuppliesField.text = s.supplies;
            eNotesField.text = s.notes;
            errorText = "";
            open();
        }

        width: 560
        title: qsTr("Fiche fournisseur")
        acceptText: qsTr("Enregistrer")
        acceptEnabled: eNameField.text.trim().length > 0
        onAcceptClicked: Suppliers.updateSupplier({
            id: supplierId,
            name: eNameField.text,
            phone: ePhoneField.text,
            email: eEmailField.text,
            address: eAddressField.text,
            taxId: eTaxField.text,
            paymentTerms: eTermsField.text,
            supplies: eSuppliesField.text,
            notes: eNotesField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Raison sociale *") }
            NTextField {
                id: eNameField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Téléphone") }
                    NTextField {
                        id: ePhoneField
                        Layout.preferredWidth: 160
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Email") }
                    NTextField {
                        id: eEmailField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Matricule fiscal") }
                    NTextField {
                        id: eTaxField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                    }
                }
            }

            NFieldLabel { text: qsTr("Adresse exacte") }
            NTextField {
                id: eAddressField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. Route de Soliman km 3, Menzel Bouzelfa — servira à la localisation sur carte")
            }

            NFieldLabel { text: qsTr("Produits / services fournis") }
            NTextField {
                id: eSuppliesField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. plants d'agrumes, terreau, transport frigorifique…")
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Conditions de paiement") }
                    NTextField {
                        id: eTermsField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("ex. 30 j fin de mois, chèque")
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Notes") }
                    NTextField {
                        id: eNotesField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            // ── Produits fournis (F07-06/07) : liens catalogue + stats
            //    d'approvisionnement calculées depuis les réceptions ──
            NFieldLabel { text: qsTr("Produits fournis (catalogue)") }
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2
                VariantPicker {
                    id: spPicker
                    Layout.fillWidth: true
                    placeholderText: qsTr("Lier un produit du catalogue…")
                    searchFunction: function (term) { return Stock.searchVariants(term); }
                }
                NButton {
                    text: qsTr("＋")
                    variant: "secondary"
                    implicitWidth: 40
                    enabled: spPicker.selectedVariantId > 0
                    onClicked: {
                        if (Suppliers.linkProduct(editDialog.supplierId,
                                                  spPicker.selectedVariantId)) {
                            spPicker.reset();
                            editDialog.reloadProducts();
                        }
                    }
                }
            }

            Repeater {
                model: editDialog.products

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.preferredWidth: 210
                        text: modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    NBadge {
                        visible: !modelData.linked
                        text: qsTr("livré, non lié")
                        badgeColor: NTheme.warning
                    }
                    Text {
                        visible: modelData.deliveryCount > 0
                        text: qsTr("%1 fournis · %2 livr.")
                            .arg(modelData.qtySupplied)
                            .arg(modelData.deliveryCount)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        visible: modelData.lastCost.length > 0
                        text: qsTr("dernier %1 · moyen %2")
                            .arg(modelData.lastCost).arg(modelData.avgCost)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.primary
                    }
                    NButton {
                        visible: !modelData.linked
                        text: qsTr("Lier")
                        variant: "secondary"
                        implicitHeight: 26
                        onClicked: {
                            if (Suppliers.linkProduct(editDialog.supplierId,
                                                      modelData.variantId))
                                editDialog.reloadProducts();
                        }
                    }
                    NButton {
                        visible: modelData.linked
                        text: qsTr("−")
                        variant: "ghost"
                        implicitWidth: 28
                        implicitHeight: 26
                        onClicked: {
                            if (Suppliers.unlinkProduct(editDialog.supplierId,
                                                        modelData.variantId))
                                editDialog.reloadProducts();
                        }
                    }
                }
            }
            Text {
                visible: editDialog.products.length === 0
                text: qsTr("Aucun produit lié ni livré pour l'instant.")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }

            // Désactivation réversible ; suppression physique UNIQUEMENT si
            // jamais référencé (ajout fautif — norme).
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: NTheme.s2

                NButton {
                    text: editDialog.supplierActive
                        ? qsTr("🚫 Désactiver")
                        : qsTr("↩ Réactiver")
                    variant: editDialog.supplierActive ? "ghost" : "secondary"
                    onClicked: {
                        if (Suppliers.setSupplierActive(editDialog.supplierId,
                                                        !editDialog.supplierActive))
                            editDialog.close();
                    }
                }
                NButton {
                    visible: editDialog.deletable
                    text: editDialog.confirmDelete
                        ? qsTr("⚠ Confirmer la suppression ?")
                        : qsTr("🗑 Supprimer (jamais utilisé)")
                    variant: "ghost"
                    onClicked: {
                        if (!editDialog.confirmDelete) {
                            editDialog.confirmDelete = true;
                        } else if (Suppliers.deleteSupplier(editDialog.supplierId)) {
                            editDialog.close();
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: editDialog.deletable
                        ? qsTr("Fiche jamais utilisée : suppression possible.")
                        : qsTr("La fiche a un historique : désactivation seulement.")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }
        }
    }

    // ── Réception de marchandise (F07-04) ─────────────────────
    NDialog {
        id: receiptDialog

        property var lines: []

        function openNew() {
            clearForm();
            open();
        }

        function clearForm() {
            lines = [];
            picker.reset();
            qtyField.text = "";
            costField.text = "";
            errorText = "";
        }

        function totalDisplay() {
            var total = 0;
            for (var i = 0; i < lines.length; ++i)
                total += lines[i].qty * lines[i].costMillimes;
            return (total / 1000).toFixed(3).replace(".", ",") + " DT";
        }

        width: 560
        title: qsTr("Réception de marchandise")
        acceptText: qsTr("Valider la réception")
        acceptEnabled: lines.length > 0
        onCancelClicked: clearForm()
        onAcceptClicked: Suppliers.recordReceipt({
            supplierId: supplierBox.currentValue,
            locationId: locationBox.currentValue,
            lines: lines,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Fournisseur") }
                    NComboBox {
                        id: supplierBox
                        Layout.preferredWidth: 240
                        textRole: "label"
                        valueRole: "id"
                        model: [{ id: 0, label: qsTr("(sans fournisseur)") }]
                            .concat(Suppliers.suppliers.map(function (s) {
                                return { id: s.id, label: s.name };
                            }))
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Vers l'emplacement") }
                    NComboBox {
                        id: locationBox
                        Layout.preferredWidth: 200
                        textRole: "label"
                        valueRole: "id"
                        model: Stock.locationOptions()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            // Saisie d'une ligne : produit + qté + coût -> Ajouter
            NFieldLabel { text: qsTr("Ajouter une ligne") }
            VariantPicker {
                id: picker
                Layout.fillWidth: true
                searchFunction: function (term) { return Stock.searchVariants(term); }
            }
            Text {
                visible: picker.selectedVariantId === 0
                text: qsTr("⚠ L'article doit exister au catalogue : tapez 2 lettres puis choisissez-le dans les suggestions.")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: NTheme.s2

                NTextField {
                    id: qtyField
                    Layout.preferredWidth: 90
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Qté")
                    validator: IntValidator { bottom: 1 }
                }
                NTextField {
                    id: costField
                    Layout.preferredWidth: 130
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Coût unit. DT")
                }
                NButton {
                    text: qsTr("＋ Ajouter")
                    variant: "secondary"
                    enabled: picker.selectedVariantId > 0
                             && parseInt(qtyField.text) > 0
                             && costField.text.length > 0
                    onClicked: {
                        var updated = receiptDialog.lines.slice();
                        updated.push({
                            variantId: picker.selectedVariantId,
                            label: picker.selectedLabel,
                            qty: parseInt(qtyField.text),
                            cost: costField.text,
                            costMillimes: Math.round(
                                parseFloat(costField.text.replace(",", ".")) * 1000),
                        });
                        receiptDialog.lines = updated;
                        picker.reset();
                        qtyField.text = "";
                        costField.text = "";
                    }
                }
                Item { Layout.fillWidth: true }
            }

            // Lignes ajoutées
            Repeater {
                model: receiptDialog.lines

                delegate: RowLayout {
                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.fillWidth: true
                        text: modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: qsTr("× %1").arg(modelData.qty)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: qsTr("à %1 DT").arg(modelData.cost)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NIconButton {
                        text: "✕"
                        implicitHeight: 26
                        onClicked: {
                            var updated = receiptDialog.lines.slice();
                            updated.splice(index, 1);
                            receiptDialog.lines = updated;
                        }
                    }
                }
            }

            Text {
                visible: receiptDialog.lines.length > 0
                text: qsTr("Total : %1").arg(receiptDialog.totalDisplay())
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSubtitle
                font.weight: Font.Bold
                color: NTheme.primaryDark
            }
        }
    }

    // ── Nouvelle commande d'achat (F07-02) ────────────────────
    NDialog {
        id: orderDialog

        property var lines: []

        function openNew() {
            clearForm();
            open();
        }

        function clearForm() {
            lines = [];
            poPicker.reset();
            poQtyField.text = "";
            poCostField.text = "";
            noteField.text = "";
            errorText = "";
        }

        function totalDisplay() {
            var total = 0;
            for (var i = 0; i < lines.length; ++i)
                total += lines[i].qty * lines[i].costMillimes;
            return (total / 1000).toFixed(3).replace(".", ",") + " DT";
        }

        // Fournisseur choisi via « Qui peut fournir ? » : sélectionne le
        // combo et re-prime les coûts des lignes avec SES derniers prix.
        function applySupplier(supplierId) {
            poSupplierBox.currentIndex =
                Math.max(0, poSupplierBox.indexOfValue(supplierId));
            var updated = lines.slice();
            for (var i = 0; i < updated.length; ++i) {
                const last = Suppliers.lastCost(supplierId, updated[i].variantId);
                if (last.length > 0) {
                    updated[i].cost = last;
                    updated[i].costMillimes = Math.round(
                        parseFloat(last.replace(",", ".")) * 1000);
                }
            }
            lines = updated;
        }

        width: 580
        title: qsTr("Nouvelle commande d'achat")
        acceptText: qsTr("Créer la commande")
        acceptEnabled: lines.length > 0 && poSupplierBox.currentValue > 0
        onCancelClicked: clearForm()
        onAcceptClicked: Suppliers.createOrder({
            supplierId: poSupplierBox.currentValue,
            note: noteField.text,
            lines: lines,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Fournisseur *") }
            RowLayout {
                spacing: NTheme.s2

                NComboBox {
                    id: poSupplierBox
                    Layout.preferredWidth: 300
                    textRole: "label"
                    valueRole: "id"
                    model: [{ id: 0, label: qsTr("— choisir —") }]
                        .concat(Suppliers.suppliers.map(function (s) {
                            return { id: s.id, label: s.name };
                        }))
                }
                // Préparation de commande : comparer qui peut fournir
                // les lignes saisies (prix / quantité déjà fournie).
                NButton {
                    text: qsTr("🔍 Qui peut fournir ?")
                    variant: "ghost"
                    enabled: orderDialog.lines.length > 0
                    onClicked: matchDialog.openFor()
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: orderDialog.lines.length === 0
                Layout.fillWidth: true
                text: qsTr("Ajoutez d'abord les lignes, puis comparez les fournisseurs possibles.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            NFieldLabel { text: qsTr("Ajouter une ligne") }
            VariantPicker {
                id: poPicker
                Layout.fillWidth: true
                searchFunction: function (term) { return Stock.searchVariants(term); }
                // F07-08 : dernier prix d'achat chez CE fournisseur pré-rempli
                // (onSelectedVariantIdChanged, pas onPicked — voir offerPicker)
                onSelectedVariantIdChanged: {
                    if (selectedVariantId <= 0)
                        return;
                    if (poSupplierBox.currentValue > 0 && poCostField.text.length === 0) {
                        const last = Suppliers.lastCost(poSupplierBox.currentValue,
                                                        selectedVariantId);
                        if (last.length > 0)
                            poCostField.text = last;
                    }
                    if (poQtyField.text.length === 0)
                        poQtyField.text = "1";
                }
            }
            Text {
                visible: poPicker.selectedVariantId === 0
                text: qsTr("⚠ L'article doit exister au catalogue : tapez 2 lettres puis choisissez-le dans les suggestions. S'il est nouveau, créez-le d'abord dans Catalogue.")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: NTheme.s2

                NTextField {
                    id: poQtyField
                    Layout.preferredWidth: 90
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Qté")
                    validator: IntValidator { bottom: 1 }
                }
                NTextField {
                    id: poCostField
                    Layout.preferredWidth: 130
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Coût unit. DT")
                }
                NButton {
                    text: qsTr("＋ Ajouter")
                    variant: "secondary"
                    enabled: poPicker.selectedVariantId > 0
                             && parseInt(poQtyField.text) > 0
                             && poCostField.text.length > 0
                    onClicked: {
                        var updated = orderDialog.lines.slice();
                        updated.push({
                            variantId: poPicker.selectedVariantId,
                            label: poPicker.selectedLabel,
                            qty: parseInt(poQtyField.text),
                            cost: poCostField.text,
                            costMillimes: Math.round(
                                parseFloat(poCostField.text.replace(",", ".")) * 1000),
                        });
                        orderDialog.lines = updated;
                        poPicker.reset();
                        poQtyField.text = "";
                        poCostField.text = "";
                    }
                }
                Item { Layout.fillWidth: true }
            }

            Repeater {
                model: orderDialog.lines

                delegate: RowLayout {
                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.fillWidth: true
                        text: modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: qsTr("× %1 à %2 DT").arg(modelData.qty).arg(modelData.cost)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NIconButton {
                        text: "✕"
                        implicitHeight: 26
                        onClicked: {
                            var updated = orderDialog.lines.slice();
                            updated.splice(index, 1);
                            orderDialog.lines = updated;
                        }
                    }
                }
            }

            NFieldLabel { text: qsTr("Note"); visible: true }
            NTextField {
                id: noteField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("conditions, délai de livraison…")
            }

            Text {
                visible: orderDialog.lines.length > 0
                text: qsTr("Total : %1").arg(orderDialog.totalDisplay())
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSubtitle
                font.weight: Font.Bold
                color: NTheme.primaryDark
            }
        }
    }

    // ── Réception d'une commande (F07-03) ─────────────────────
    NDialog {
        id: poReceiveDialog

        property var detail: ({ lines: [] })
        property var recvQty: ({})

        function openFor(poId) {
            detail = Suppliers.orderDetail(poId);
            var q = {};
            for (var i = 0; i < detail.lines.length; ++i)
                q[detail.lines[i].variantId] = detail.lines[i].remaining;
            recvQty = q;
            errorText = "";
            open();
        }

        function collectLines() {
            var out = [];
            for (var key in recvQty) {
                var qty = parseInt(recvQty[key]) || 0;
                if (qty > 0) out.push({ variantId: parseInt(key), qty: qty });
            }
            return out;
        }

        width: 600
        title: qsTr("Réception — %1").arg(detail.number || "")
        acceptText: qsTr("Valider la réception")
        acceptEnabled: poLocationBox.currentValue > 0
        onAcceptClicked: Suppliers.receiveOrder({
            poId: detail.id,
            locationId: poLocationBox.currentValue,
            lines: collectLines(),
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s2
                Text {
                    text: poReceiveDialog.detail.supplierName || ""
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: NTheme.textSecondary
                }
                Item { Layout.fillWidth: true }
                NFieldLabel { text: qsTr("Vers l'emplacement") }
                NComboBox {
                    id: poLocationBox
                    Layout.preferredWidth: 200
                    textRole: "label"
                    valueRole: "id"
                    model: Stock.locationOptions()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            // En-tête colonnes
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Article")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 130
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Reçu / Commandé")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 90
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("À recevoir")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            Repeater {
                model: poReceiveDialog.detail.lines

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.fillWidth: true
                        text: modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        Layout.preferredWidth: 130
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("%1 / %2").arg(modelData.qtyReceived).arg(modelData.qtyOrdered)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NTextField {
                        Layout.preferredWidth: 90
                        fieldColor: NTheme.surface
                        enabled: modelData.remaining > 0
                        text: modelData.remaining
                        horizontalAlignment: Text.AlignRight
                        validator: IntValidator { bottom: 0; top: modelData.remaining }
                        onTextEdited: {
                            var q = poReceiveDialog.recvQty;
                            q[modelData.variantId] = text;
                            poReceiveDialog.recvQty = q;
                        }
                    }
                }
            }
        }
    }

    // ── Paiement fournisseur (F07-05) ─────────────────────────
    NDialog {
        id: paySupplierDialog

        property var supplier: null
        property var history: []

        function openFor(s) {
            supplier = s;
            history = Suppliers.supplierPayments(s.id);
            payAmountField.text = "";
            payChequeField.text = "";
            payDueField.text = "";
            payNoteField.text = "";
            payMethodBox.currentIndex = 0;
            errorText = "";
            open();
        }

        width: 500
        title: supplier
            ? qsTr("Payer — %1 (dette %2)")
                  .arg(supplier.name).arg(supplier.balanceDisplay)
            : ""
        acceptText: qsTr("Enregistrer le paiement")
        acceptEnabled: payAmountField.text.trim().length > 0
                       && (payMethodBox.currentValue !== "cheque"
                           || payDueField.dateValid)
        onOpened: payAmountField.forceActiveFocus()
        onAcceptClicked: Suppliers.paySupplier({
            supplierId: supplier ? supplier.id : 0,
            amount: payAmountField.text,
            method: payMethodBox.currentValue,
            chequeNumber: payChequeField.text,
            chequeDue: payDueField.text,
            note: payNoteField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Montant (DT) *") }
                    NTextField {
                        id: payAmountField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        font.pixelSize: NTheme.fontSizeTitle
                        placeholderText: "120,000"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Mode") }
                    NComboBox {
                        id: payMethodBox
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("💵 Espèces"), value: "cash" },
                            { label: qsTr("🧾 Chèque"), value: "cheque" },
                            { label: qsTr("🏦 Virement"), value: "transfer" },
                        ]
                    }
                }
            }

            RowLayout {
                visible: payMethodBox.currentValue === "cheque"
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("N° de chèque") }
                    NTextField {
                        id: payChequeField
                        Layout.preferredWidth: 160
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Échéance *") }
                    NDateField {
                        id: payDueField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        required: true
                    }
                }
            }

            NFieldLabel { text: qsTr("Note") }
            NTextField {
                id: payNoteField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. acompte livraison agrumes")
            }

            // Derniers paiements à ce fournisseur
            Rectangle {
                visible: paySupplierDialog.history.length > 0
                Layout.fillWidth: true; height: 1; color: NTheme.outline
            }
            Repeater {
                model: paySupplierDialog.history

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        text: modelData.date
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.method
                            + (modelData.chequeNumber.length > 0
                               ? " n° " + modelData.chequeNumber : "")
                            + (modelData.chequeDue.length > 0
                               ? qsTr(" · éch. %1").arg(modelData.chequeDue) : "")
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        text: modelData.amount
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.primary
                    }
                }
            }
        }
    }

    // ── Échéances de chèques (F07-05) ─────────────────────────
    NDialog {
        id: dueDialog

        property var cheques: []

        function openNew() {
            cheques = Suppliers.dueCheques();
            errorText = "";
            open();
        }

        width: 520
        title: qsTr("Chèques à échéance (30 jours)")
        acceptText: qsTr("Fermer")
        showCancel: false
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Repeater {
                model: dueDialog.cheques

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    NBadge {
                        text: modelData.overdue ? qsTr("ÉCHU") : modelData.dueDate
                        badgeColor: modelData.overdue ? NTheme.danger : NTheme.info
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.supplierName
                            + (modelData.chequeNumber.length > 0
                               ? " · n° " + modelData.chequeNumber : "")
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        visible: modelData.overdue
                        text: modelData.dueDate
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.danger
                    }
                    Text {
                        text: modelData.amount
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.Bold
                        color: NTheme.primary
                    }
                }
            }

            NEmptyState {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                visible: dueDialog.cheques.length === 0
                emoji: "✅"
                message: qsTr("Aucun chèque à échéance dans les 30 jours.")
            }
        }
    }

    // ── Meilleure offre + évolution des prix (F07-09/10) ──────
    NDialog {
        id: offerDialog

        property var offers: []
        property var pricePoints: []

        function openNew() {
            offerPicker.reset();
            offers = [];
            pricePoints = [];
            errorText = "";
            open();
        }
        function reload() {
            if (offerPicker.selectedVariantId > 0) {
                offers = Suppliers.offersFor(offerPicker.selectedVariantId);
                pricePoints = Suppliers.priceHistory(offerPicker.selectedVariantId);
            } else {
                offers = [];
                pricePoints = [];
            }
        }

        width: 640
        title: qsTr("Meilleure offre fournisseur")
        acceptText: qsTr("Fermer")
        showCancel: false
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            VariantPicker {
                id: offerPicker
                Layout.fillWidth: true
                placeholderText: qsTr("Quel produit cherchez-vous à acheter ?")
                searchFunction: function (term) { return Stock.searchVariants(term); }
                // onSelectedVariantIdChanged (pas onPicked : signal paramétré
                // non délivré de façon fiable — bug constaté)
                onSelectedVariantIdChanged: offerDialog.reload()
            }

            // Classement des fournisseurs, meilleur dernier prix d'abord
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2
                visible: offerDialog.offers.length > 0
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Fournisseur")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 105
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Dernier prix")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 105
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Prix moyen")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 130
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Fournis · dernière")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            Repeater {
                model: offerDialog.offers

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: NTheme.s2
                        Text {
                            text: (modelData.best ? "🏆 " : "") + modelData.supplierName
                            elide: Text.ElideRight
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: modelData.best ? Font.Bold : Font.Normal
                            color: NTheme.textPrimary
                        }
                        NBadge {
                            visible: modelData.deliveryCount === 0
                            text: qsTr("jamais livré")
                            badgeColor: NTheme.textSecondary
                        }
                    }
                    Text {
                        Layout.preferredWidth: 105
                        horizontalAlignment: Text.AlignRight
                        text: modelData.lastCost || "—"
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: modelData.best ? Font.Bold : Font.Normal
                        color: modelData.best ? NTheme.success : NTheme.textPrimary
                    }
                    Text {
                        Layout.preferredWidth: 105
                        horizontalAlignment: Text.AlignRight
                        text: modelData.avgCost || "—"
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textSecondary
                    }
                    Text {
                        Layout.preferredWidth: 130
                        horizontalAlignment: Text.AlignRight
                        text: modelData.deliveryCount > 0
                            ? qsTr("%1 · %2").arg(modelData.qtySupplied)
                                  .arg(modelData.lastDelivery)
                            : "—"
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                }
            }

            NEmptyState {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                visible: offerPicker.selectedVariantId > 0
                         && offerDialog.offers.length === 0
                emoji: "🔍"
                message: qsTr("Aucun fournisseur connu pour ce produit — liez-en un dans sa fiche.")
            }

            // Évolution du prix d'achat (F07-10)
            ColumnLayout {
                Layout.fillWidth: true
                visible: offerDialog.pricePoints.length > 1
                spacing: NTheme.s1

                NFieldLabel { text: qsTr("Évolution du prix d'achat") }
                NLineChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 130
                    points: offerDialog.pricePoints
                }
            }
        }
    }

    // ── Préparation de commande : qui peut fournir ? ──────────
    NDialog {
        id: matchDialog

        property var rows: []
        property string sortBy: "price"

        function reload() {
            rows = Suppliers.supplierMatches(orderDialog.lines, sortBy);
        }
        function openFor() {
            sortBy = "price";
            sortBox.currentIndex = 0;
            reload();
            open();
        }

        width: 600
        title: qsTr("Qui peut fournir cette commande ?")
        acceptText: qsTr("Fermer")
        cancelText: ""
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s2

                NFieldLabel { text: qsTr("Trier par") }
                NComboBox {
                    id: sortBox
                    Layout.preferredWidth: 220
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("Prix estimé"), value: "price" },
                        { label: qsTr("Quantité déjà fournie"), value: "quantity" },
                    ]
                    onActivated: {
                        matchDialog.sortBy = currentValue;
                        matchDialog.reload();
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("🗺 distance : bientôt, avec la carte")
                    elide: Text.ElideRight
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            ListView {
                id: matchList

                Layout.fillWidth: true
                Layout.preferredHeight: count === 0
                    ? 110 : Math.min(count, 7) * 56 + 8
                clip: true
                model: matchDialog.rows
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: matchRow

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 56
                    showSeparator: index < matchList.count - 1
                    // Clic = ce fournisseur pour la commande + ses prix
                    onClicked: {
                        orderDialog.applySupplier(matchRow.modelData.supplierId);
                        matchDialog.close();
                    }

                    Text {
                        visible: matchRow.index === 0 && matchRow.modelData.full
                        text: "🏆"
                        font.pixelSize: 16
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        RowLayout {
                            spacing: NTheme.s2
                            Text {
                                text: matchRow.modelData.name
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: NTheme.textPrimary
                            }
                            NBadge {
                                text: qsTr("%1 produits").arg(matchRow.modelData.coverageLabel)
                                badgeColor: matchRow.modelData.full
                                    ? NTheme.success : NTheme.warning
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: matchRow.modelData.full
                                ? qsTr("%n unité(s) déjà livrée(s) sur ces produits", "",
                                       matchRow.modelData.suppliedQty)
                                : qsTr("manque : %1").arg(matchRow.modelData.missingLabels)
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: matchRow.modelData.full
                                ? NTheme.textSecondary : NTheme.warning
                        }
                    }
                    Text {
                        text: matchRow.modelData.estimatedDisplay
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: matchRow.modelData.priceComplete
                            ? NTheme.primary : NTheme.textSecondary
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: matchList.count === 0
                    emoji: "🚚"
                    message: qsTr("Aucun fournisseur connu pour ces produits — liez-les depuis une fiche fournisseur ou réceptionnez une première livraison.")
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Choisir un fournisseur reprend ses derniers prix d'achat dans les lignes de la commande.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }
}
