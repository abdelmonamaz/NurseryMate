import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Devis (M05) : liste, création (lignes produit ou libres),
// PDF, changement de statut (F05-01/02).
Item {
    id: page

    // Émis quand un devis est chargé en caisse — Main.qml bascule l'écran.
    signal navigateToSales()

    Component.onCompleted: Quotes.refresh()

    Connections {
        target: Quotes
        function onQuoteCreated(id) {
            createDialog.close();
            createDialog.clearForm();
            const url = Quotes.pdfFor(id);
            if (url.length > 0) Qt.openUrlExternally(url);
        }
        function onErrorOccurred(message) {
            if (createDialog.opened) createDialog.errorText = message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s3

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Devis")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSubtitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }
            Item { Layout.fillWidth: true }
            NButton {
                text: qsTr("🧾 Nouveau devis")
                onClicked: createDialog.open()
            }
        }

        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: quoteList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Quotes.quotes
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: qRow
                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 60
                    showSeparator: index < quoteList.count - 1
                    // Clic sur la ligne = détail du devis
                    onClicked: detailDialog.openFor(qRow.modelData.id)

                    ColumnLayout {
                        Layout.preferredWidth: 220
                        spacing: 2
                        RowLayout {
                            spacing: NTheme.s2
                            Text {
                                text: qRow.modelData.number
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                            NBadge {
                                text: qRow.modelData.statusLabel
                                badgeColor: qRow.modelData.status === "accepted" ? NTheme.success
                                    : qRow.modelData.status === "refused" ? NTheme.danger
                                    : qRow.modelData.status === "sent" ? NTheme.info
                                    : NTheme.textSecondary
                            }
                        }
                        Text {
                            text: qRow.modelData.customerName
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    Text {
                        text: qsTr("Valable jusqu'au %1").arg(qRow.modelData.validUntil)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qRow.modelData.total
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.primary
                    }
                    NIconButton {
                        text: "📄"
                        implicitHeight: 36
                        onClicked: {
                            const url = Quotes.pdfFor(qRow.modelData.id);
                            if (url.length > 0) Qt.openUrlExternally(url);
                        }
                    }
                    NButton {
                        text: qsTr("Accepté")
                        variant: "secondary"
                        visible: qRow.modelData.status !== "accepted"
                        onClicked: Quotes.setStatus(qRow.modelData.id, "accepted")
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: quoteList.count === 0
                    emoji: "🧾"
                    message: qsTr("Aucun devis — créez-en un pour un client professionnel.")
                    actionText: qsTr("🧾 Nouveau devis")
                    onActionClicked: createDialog.open()
                }
            }
        }
    }

    // ── Nouveau devis (F05-01) ────────────────────────────────
    NDialog {
        id: createDialog

        property var lines: []

        function clearForm() {
            lines = [];
            customerField.text = "";
            discountField.text = "";
            noteField.text = "";
            picker.reset();
            labelField.text = "";
            qtyField.text = "";
            priceField.text = "";
            errorText = "";
        }

        function totalDisplay() {
            var t = 0;
            for (var i = 0; i < lines.length; ++i)
                t += lines[i].qty * lines[i].priceMillimes;
            var disc = Math.round(parseFloat((discountField.text || "0").replace(",", ".")) * 1000) || 0;
            return ((t - disc) / 1000).toFixed(3).replace(".", ",") + " DT";
        }

        width: 600
        title: qsTr("Nouveau devis")
        acceptText: qsTr("Créer et ouvrir le PDF")
        acceptEnabled: lines.length > 0
        onCancelClicked: clearForm()
        onAcceptClicked: Quotes.createQuote({
            customerName: customerField.text,
            discount: discountField.text,
            note: noteField.text,
            lines: lines,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Client") }
            NTextField {
                id: customerField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("Nom du client ou prospect")
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            // Ajout d'une ligne : produit (auto-remplit) ou prestation libre
            NFieldLabel { text: qsTr("Ajouter une ligne (produit ou prestation)") }
            VariantPicker {
                id: picker
                Layout.fillWidth: true
                searchFunction: function (term) { return Quotes.searchVariants(term); }
                // Pré-remplit libellé + prix catalogue à la sélection
                // (onSelectedVariantIdChanged : onPicked non fiable, bug constaté)
                onSelectedVariantIdChanged: {
                    if (selectedVariantId <= 0 || !selectedData)
                        return;
                    labelField.text = selectedData.label;
                    priceField.text = (selectedData.priceMillimes / 1000)
                        .toFixed(3).replace(".", ",");
                    if (qtyField.text.length === 0)
                        qtyField.text = "1";
                }
            }
            Text {
                text: qsTr("Choisissez un produit dans les suggestions (prix pré-rempli), ou saisissez librement une prestation ci-dessous.")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: NTheme.s2
                NTextField {
                    id: labelField
                    Layout.fillWidth: true
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Désignation (ex. Plantation d'oliviers)")
                }
                NTextField {
                    id: qtyField
                    Layout.preferredWidth: 70
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("Qté")
                    validator: IntValidator { bottom: 1 }
                }
                NTextField {
                    id: priceField
                    Layout.preferredWidth: 110
                    fieldColor: NTheme.surface
                    placeholderText: qsTr("P.U. DT")
                }
                NButton {
                    text: qsTr("＋")
                    variant: "secondary"
                    enabled: labelField.text.length > 0 && parseInt(qtyField.text) > 0
                             && priceField.text.length > 0
                    onClicked: {
                        var u = createDialog.lines.slice();
                        u.push({
                            variantId: picker.selectedVariantId,
                            label: labelField.text,
                            qty: parseInt(qtyField.text),
                            price: priceField.text,
                            priceMillimes: Math.round(parseFloat(priceField.text.replace(",", ".")) * 1000),
                        });
                        createDialog.lines = u;
                        picker.reset(); labelField.text = ""; qtyField.text = ""; priceField.text = "";
                    }
                }
            }

            Repeater {
                model: createDialog.lines
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
                        text: qsTr("%1 × %2 DT").arg(modelData.qty).arg(modelData.price)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NIconButton {
                        text: "✕"
                        implicitHeight: 24
                        onClicked: {
                            var u = createDialog.lines.slice();
                            u.splice(index, 1);
                            createDialog.lines = u;
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Remise (DT)") }
                    NTextField {
                        id: discountField
                        Layout.preferredWidth: 110
                        fieldColor: NTheme.surface
                        placeholderText: "0,000"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Note") }
                    NTextField {
                        id: noteField
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("conditions, délais…")
                    }
                }
                ColumnLayout {
                    spacing: 0
                    NFieldLabel { Layout.alignment: Qt.AlignRight; text: qsTr("TOTAL") }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: createDialog.totalDisplay()
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.primaryDark
                    }
                }
            }
        }
    }

    // ── Détail d'un devis (clic sur une ligne) ────────────────
    NDialog {
        id: detailDialog

        property var detail: ({ lines: [] })

        function openFor(quoteId) {
            detail = Quotes.quoteDetail(quoteId);
            errorText = "";
            open();
        }
        function changeStatus(status) {
            if (Quotes.setStatus(detail.id, status))
                detail = Quotes.quoteDetail(detail.id);
        }

        width: 600
        title: qsTr("Devis %1").arg(detail.number || "")
        acceptText: qsTr("Fermer")
        showCancel: false
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2

                NBadge {
                    text: detailDialog.detail.statusLabel || ""
                    badgeColor: detailDialog.detail.status === "accepted" ? NTheme.success
                        : detailDialog.detail.status === "refused" ? NTheme.danger
                        : detailDialog.detail.status === "sent" ? NTheme.info
                        : NTheme.textSecondary
                }
                Text {
                    Layout.fillWidth: true
                    text: detailDialog.detail.customerName || ""
                    elide: Text.ElideRight
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: NTheme.textPrimary
                }
                Text {
                    text: qsTr("créé le %1 · valable jusqu'au %2")
                        .arg(detailDialog.detail.createdAt || "")
                        .arg(detailDialog.detail.validUntil || "")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            // Lignes du devis
            Repeater {
                model: detailDialog.detail.lines
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.fillWidth: true
                        text: (modelData.isFree ? "🛠 " : "") + modelData.label
                        elide: Text.ElideRight
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: qsTr("%1 × %2").arg(modelData.qty).arg(modelData.unitPrice)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        Layout.preferredWidth: 110
                        horizontalAlignment: Text.AlignRight
                        text: modelData.lineTotal
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.primary
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    visible: (detailDialog.detail.note || "").length > 0
                    Layout.fillWidth: true
                    text: detailDialog.detail.note || ""
                    elide: Text.ElideRight
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Item { Layout.fillWidth: true }
                ColumnLayout {
                    spacing: 0
                    Text {
                        visible: detailDialog.detail.hasDiscount === true
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Remise : −%1").arg(detailDialog.detail.discount || "")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: detailDialog.detail.total || ""
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeTitle
                        font.weight: Font.Bold
                        color: NTheme.primaryDark
                    }
                }
            }

            // Actions : PDF + cycle de statut du devis
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2

                NButton {
                    text: qsTr("📄 PDF")
                    variant: "secondary"
                    onClicked: {
                        const url = Quotes.pdfFor(detailDialog.detail.id);
                        if (url.length > 0) Qt.openUrlExternally(url);
                    }
                }
                // Conversion : devis accepté -> panier de caisse (F05)
                NButton {
                    text: qsTr("🛒 Encaisser")
                    visible: detailDialog.detail.status === "accepted"
                    onClicked: {
                        if (Sales.loadQuote(detailDialog.detail.id)) {
                            detailDialog.close();
                            page.navigateToSales();
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                // Correction d'une fausse manipulation : un devis accepté /
                // refusé / expiré peut être rouvert (retour à Envoyé).
                NButton {
                    text: qsTr("↩ Rouvrir")
                    variant: "ghost"
                    visible: detailDialog.detail.status === "accepted"
                             || detailDialog.detail.status === "refused"
                             || detailDialog.detail.status === "expired"
                    onClicked: detailDialog.changeStatus("sent")
                }
                NButton {
                    text: qsTr("Envoyé")
                    variant: "ghost"
                    visible: detailDialog.detail.status === "draft"
                    onClicked: detailDialog.changeStatus("sent")
                }
                NButton {
                    text: qsTr("Refusé")
                    variant: "ghost"
                    visible: detailDialog.detail.status === "draft"
                             || detailDialog.detail.status === "sent"
                    onClicked: detailDialog.changeStatus("refused")
                }
                NButton {
                    text: qsTr("✓ Accepté")
                    visible: detailDialog.detail.status === "draft"
                             || detailDialog.detail.status === "sent"
                    onClicked: detailDialog.changeStatus("accepted")
                }
            }
        }
    }
}
