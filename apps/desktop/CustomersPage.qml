import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Clients (M06) : fiches, encours colorés, règlements (F06-01..04).
Item {
    id: page

    Component.onCompleted: Customers.refresh()

    Connections {
        target: Customers

        function onCustomerCreated(customerId) {
            addDialog.close();
            addDialog.clearForm();
        }
        function onPaymentRecorded() {
            payDialog.close();
            payDialog.clearForm();
        }
        function onCustomerUpdated() {
            editDialog.close();
        }
        function onErrorOccurred(message) {
            if (addDialog.opened)
                addDialog.errorText = message;
            else if (editDialog.opened)
                editDialog.errorText = message;
            else if (payDialog.opened)
                payDialog.errorText = message;
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
                placeholderText: qsTr("Rechercher (nom, téléphone)…")
                onTextEdited: Customers.searchTerm = text
            }

            NButton {
                text: Customers.showInactive ? qsTr("👁 Inactifs affichés")
                                             : qsTr("Voir inactifs")
                variant: "ghost"
                onClicked: Customers.showInactive = !Customers.showInactive
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("%n client(s)", "", customerList.count)
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textSecondary
            }

            NButton {
                text: qsTr("＋ Nouveau client")
                onClicked: addDialog.open()
            }
        }

        // ── Liste des clients ─────────────────────────────────
        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: customerList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Customers.customers
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: row

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 60
                    showSeparator: index < customerList.count - 1
                    // Clic = fiche client éditable (F06-01)
                    onClicked: editDialog.openFor(row.modelData.id)

                    Text {
                        text: row.modelData.professional ? "🏢" : "👤"
                        font.pixelSize: 22
                    }

                    ColumnLayout {
                        spacing: 2

                        Text {
                            text: row.modelData.name
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            visible: row.modelData.phone.length > 0
                            text: row.modelData.phone
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    NBadge {
                        visible: row.modelData.professional
                        text: qsTr("Pro")
                        badgeColor: NTheme.info
                    }

                    NBadge {
                        visible: !row.modelData.active
                        text: qsTr("Inactif")
                        badgeColor: NTheme.danger
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        visible: row.modelData.creditLimitDisplay.length > 0
                        text: qsTr("plafond %1").arg(row.modelData.creditLimitDisplay)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }

                    ColumnLayout {
                        spacing: 0

                        NFieldLabel {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("ENCOURS")
                        }
                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: row.modelData.hasDebt
                                ? row.modelData.balanceDisplay : "—"
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: row.modelData.hasDebt ? NTheme.danger
                                                         : NTheme.textSecondary
                        }
                    }

                    NButton {
                        visible: row.modelData.hasDebt
                        text: qsTr("💰 Règlement")
                        variant: "secondary"
                        onClicked: payDialog.openFor(row.modelData)
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: customerList.count === 0
                    emoji: "👥"
                    message: qsTr("Aucun client — créez la première fiche.")
                    actionText: qsTr("＋ Ajouter un client")
                    onActionClicked: addDialog.open()
                }
            }
        }
    }

    // ── Nouveau client (F06-01) ───────────────────────────────
    NDialog {
        id: addDialog

        function clearForm() {
            nameField.text = "";
            phoneField.text = "";
            limitField.text = "";
            kindBox.currentIndex = 0;
            errorText = "";
        }

        width: 420
        title: qsTr("Nouveau client")
        acceptText: qsTr("Enregistrer")
        onOpened: nameField.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Customers.createCustomer({
            name: nameField.text,
            kind: kindBox.currentValue,
            phone: phoneField.text,
            creditLimit: limitField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Nom / raison sociale *") }
            NTextField {
                id: nameField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. Ali Jardins (paysagiste)")
            }

            NFieldLabel { text: qsTr("Type") }
            NComboBox {
                id: kindBox
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("Particulier"), value: "individual" },
                    { label: qsTr("Professionnel"), value: "professional" },
                ]
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
                        placeholderText: "22 123 456"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Plafond crédit (DT)") }
                    NTextField {
                        id: limitField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("vide = illimité")
                    }
                }
            }
        }
    }

    // ── Fiche client éditable (F06-01) ────────────────────────
    NDialog {
        id: editDialog

        property int customerId: 0
        property bool customerActive: true
        property bool deletable: false
        property bool confirmDelete: false

        function openFor(id) {
            const c = Customers.customerDetail(id);
            if (!c.id) return;
            customerId = c.id;
            customerActive = c.active;
            deletable = Customers.isDeletable(c.id);
            confirmDelete = false;
            cNameField.text = c.name;
            cKindBox.currentIndex = c.kind === "professional" ? 1 : 0;
            cPhoneField.text = c.phone;
            cPhone2Field.text = c.phone2;
            cEmailField.text = c.email;
            cAddressField.text = c.address;
            cTaxField.text = c.taxId;
            cLimitField.text = c.creditLimit;
            cNotesField.text = c.notes;
            errorText = "";
            open();
        }

        width: 540
        title: qsTr("Fiche client")
        acceptText: qsTr("Enregistrer")
        acceptEnabled: cNameField.text.trim().length > 0
        onAcceptClicked: Customers.updateCustomer({
            id: customerId,
            name: cNameField.text,
            kind: cKindBox.currentValue,
            phone: cPhoneField.text,
            phone2: cPhone2Field.text,
            email: cEmailField.text,
            address: cAddressField.text,
            taxId: cTaxField.text,
            creditLimit: cLimitField.text,
            notes: cNotesField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Nom / raison sociale *") }
                    NTextField {
                        id: cNameField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Type") }
                    NComboBox {
                        id: cKindBox
                        Layout.preferredWidth: 160
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("Particulier"), value: "individual" },
                            { label: qsTr("Professionnel"), value: "professional" },
                        ]
                    }
                }
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Téléphone") }
                    NTextField {
                        id: cPhoneField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Téléphone 2") }
                    NTextField {
                        id: cPhone2Field
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Email") }
                    NTextField {
                        id: cEmailField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                    }
                }
            }

            NFieldLabel { text: qsTr("Adresse") }
            NTextField {
                id: cAddressField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Matricule fiscal") }
                    NTextField {
                        id: cTaxField
                        Layout.preferredWidth: 160
                        fieldColor: NTheme.surface
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Plafond crédit (DT)") }
                    NTextField {
                        id: cLimitField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("vide = illimité")
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Notes") }
                    NTextField {
                        id: cNotesField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                    }
                }
            }

            // Désactivation réversible ; suppression physique UNIQUEMENT si
            // la fiche n'est référencée nulle part (ajout fautif — norme).
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: NTheme.s2

                NButton {
                    text: editDialog.customerActive
                        ? qsTr("🚫 Désactiver")
                        : qsTr("↩ Réactiver")
                    variant: editDialog.customerActive ? "ghost" : "secondary"
                    onClicked: {
                        if (Customers.setCustomerActive(editDialog.customerId,
                                                        !editDialog.customerActive))
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
                        } else if (Customers.deleteCustomer(editDialog.customerId)) {
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

    // ── Règlement sur encours (F06-03) ────────────────────────
    NDialog {
        id: payDialog

        property var customer: null

        function openFor(customerData) {
            clearForm();
            customer = customerData;
            open();
        }

        function clearForm() {
            amountField.text = "";
            chequeField.text = "";
            methodBox.currentIndex = 0;
            errorText = "";
        }

        width: 420
        title: customer
            ? qsTr("Règlement — %1 (encours %2)")
                  .arg(customer.name).arg(customer.balanceDisplay)
            : ""
        acceptText: qsTr("Encaisser")
        onOpened: amountField.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Customers.recordPayment({
            customerId: customer ? customer.id : 0,
            amount: amountField.text,
            method: methodBox.currentValue,
            chequeNumber: chequeField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Montant (DT) *") }
            NTextField {
                id: amountField
                Layout.preferredWidth: 160
                fieldColor: NTheme.surface
                font.pixelSize: NTheme.fontSizeTitle
                placeholderText: "50,000"
            }

            NFieldLabel { text: qsTr("Mode de paiement") }
            NComboBox {
                id: methodBox
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("💵 Espèces"), value: "cash" },
                    { label: qsTr("🧾 Chèque"), value: "cheque" },
                    { label: qsTr("🏦 Virement"), value: "transfer" },
                ]
            }

            ColumnLayout {
                visible: methodBox.currentValue === "cheque"
                spacing: NTheme.s1

                NFieldLabel { text: qsTr("N° de chèque") }
                NTextField {
                    id: chequeField
                    Layout.fillWidth: true
                    fieldColor: NTheme.surface
                }
            }
        }
    }
}
