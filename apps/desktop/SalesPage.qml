import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Caisse (M04) : recherche -> panier -> paiement, < 30 s par vente
// (US-04.1, doc 03 §4.2).
Item {
    id: page

    Connections {
        target: Sales

        function onSaleCompleted(number, totalDisplay, ticketUrl) {
            paymentDialog.close();
            successBanner.text =
                qsTr("✅ Vente %1 enregistrée — %2").arg(number).arg(totalDisplay);
            successBanner.ticketUrl = ticketUrl;
            successBanner.visible = true;
            searchField.forceActiveFocus();
        }
        function onErrorOccurred(message) {
            if (paymentDialog.opened)
                paymentDialog.errorText = message;
            else if (cancelDialog.opened)
                cancelDialog.errorText = message;
            else if (creditNoteDialog.opened)
                creditNoteDialog.errorText = message;
            else {
                successBanner.text = "⚠ " + message;
                successBanner.visible = true;
            }
        }
        function onSaleCancelled() {
            cancelDialog.close();
            creditNoteDialog.close();
            journalDialog.rows = Sales.todayJournal();
            journalDialog.totals = Sales.todayTotals();
            Stock.refresh();
        }
        function onCreditNoteCreated(number, pdfUrl) {
            successBanner.ticketUrl = pdfUrl;
            successBanner.text = qsTr("↩ Avoir %1 émis").arg(number);
            successBanner.visible = true;
        }
        function onClosureDone(gapDisplay, hasGap) {
            closureDialog.close();
            successBanner.ticketUrl = "";
            successBanner.text = hasGap
                ? qsTr("🔒 Clôture enregistrée — écart caisse : %1").arg(gapDisplay)
                : qsTr("🔒 Clôture enregistrée — caisse juste ✅");
            successBanner.visible = true;
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s4

        // ══ Colonne gauche : recherche produit ════════════════
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: NTheme.s3

            NTextField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTr("Produit : nom FR/AR, SKU ou douchette… [F3]")
                font.pixelSize: NTheme.fontSizeSubtitle
                onTextEdited: resultsList.results =
                    text.length >= 2 ? Sales.searchVariants(text) : []
            }

            // Bannière succès / erreur + ouverture du ticket (F04-06)
            NCard {
                id: successBanner

                property alias text: bannerText.text
                property string ticketUrl: ""

                Layout.fillWidth: true
                Layout.preferredHeight: 44
                visible: false
                color: NTheme.primaryContainer
                border.color: NTheme.leaf

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: NTheme.s3
                    anchors.rightMargin: NTheme.s2
                    spacing: NTheme.s2

                    Text {
                        id: bannerText
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    NButton {
                        visible: successBanner.ticketUrl.length > 0
                        text: qsTr("🧾 Ouvrir le ticket")
                        variant: "secondary"
                        onClicked: Qt.openUrlExternally(successBanner.ticketUrl)
                    }
                }
            }

            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: resultsList

                    property var results: []

                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    model: results
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: NListRow {
                        id: resultRow

                        required property int index
                        required property var modelData

                        width: ListView.view.width
                        height: 52
                        showSeparator: index < resultsList.count - 1
                        onClicked: {
                            Sales.addToCart(resultRow.modelData);
                            successBanner.visible = false;
                        }

                        Text {
                            text: resultRow.modelData.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            text: resultRow.modelData.sku
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: resultRow.modelData.priceDisplay
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.Bold
                            color: NTheme.primary
                        }
                        NIconButton {
                            text: "＋"
                            variant: "primary"
                            implicitHeight: 36
                            onClicked: resultRow.clicked()
                        }
                    }

                    NEmptyState {
                        anchors.centerIn: parent
                        visible: resultsList.count === 0
                        emoji: "🔍"
                        message: searchField.text.length >= 2
                            ? qsTr("Aucun produit trouvé.")
                            : qsTr("Tapez 2 lettres pour chercher un produit.")
                    }
                }
            }

            // Journal du jour (F04-07)
            RowLayout {
                Layout.fillWidth: true

                Text {
                    property var totals: Sales.todayTotals()

                    text: qsTr("Aujourd'hui : %1 vente(s) · %2")
                        .arg(totals.count === undefined ? 0 : totals.count)
                        .arg(totals.total === undefined ? "0,000 DT" : totals.total)
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    color: NTheme.textSecondary

                    Connections {
                        target: Sales
                        function onSaleCompleted() { parent.totals = Sales.todayTotals(); }
                    }
                }
                Item { Layout.fillWidth: true }
                NButton {
                    visible: Auth.currentRole === "manager"
                    text: qsTr("🔒 Clôture")
                    variant: "ghost"
                    onClicked: {
                        closureDialog.expected = Sales.expectedCashDisplay();
                        closureDialog.countedText = "";
                        closureDialog.errorText = "";
                        closureDialog.open();
                    }
                }
                NButton {
                    text: qsTr("Journal du jour")
                    variant: "ghost"
                    onClicked: {
                        journalDialog.rows = Sales.todayJournal();
                        journalDialog.totals = Sales.todayTotals();
                        journalDialog.open();
                    }
                }
            }
        }

        // ══ Colonne droite : panier ═══════════════════════════
        NCard {
            Layout.preferredWidth: 420
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: NTheme.s3
                spacing: NTheme.s2

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Vente en cours")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }
                    NBadge {
                        visible: Sales.itemCount > 0
                        text: qsTr("%n article(s)", "", Sales.itemCount)
                        badgeColor: NTheme.info
                    }
                    Item { Layout.fillWidth: true }
                    NButton {
                        visible: Sales.heldCarts.length > 0
                        text: qsTr("⏸ En attente (%1)").arg(Sales.heldCarts.length)
                        variant: "ghost"
                        onClicked: heldDialog.open()
                    }
                    NButton {
                        visible: cartList.count > 0
                        text: qsTr("⏸ Mettre en attente")
                        variant: "ghost"
                        onClicked: Sales.holdCart()
                    }
                    NButton {
                        visible: cartList.count > 0
                        text: qsTr("Vider")
                        variant: "ghost"
                        onClicked: Sales.clearCart()
                    }
                }

                // Client du panier + tarif pro auto (RG-04.c, F04-04)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        text: "👤"
                        font.pixelSize: 16
                    }
                    Text {
                        visible: Sales.cartCustomerId === 0
                        text: qsTr("Client de passage")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textSecondary
                    }
                    Text {
                        visible: Sales.cartCustomerId > 0
                        text: Sales.cartCustomerName
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.textPrimary
                    }
                    NBadge {
                        visible: Sales.proPricing
                        text: qsTr("Prix pro")
                        badgeColor: NTheme.leaf
                    }
                    Item { Layout.fillWidth: true }
                    NButton {
                        text: Sales.cartCustomerId > 0 ? qsTr("Changer") : qsTr("+ Client")
                        variant: "ghost"
                        onClicked: customerDialog.open()
                    }
                    NIconButton {
                        visible: Sales.cartCustomerId > 0
                        text: "✕"
                        implicitHeight: 30
                        onClicked: Sales.setCartCustomer(0)
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

                ListView {
                    id: cartList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Sales.cart
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 2

                    delegate: Rectangle {
                        id: cartRow

                        required property int index
                        required property string label
                        required property int qty
                        required property string unitPriceRaw
                        required property string lineTotal
                        required property bool manualPrice

                        width: ListView.view.width
                        height: 56
                        radius: NTheme.radiusButton
                        color: NTheme.surface

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: NTheme.s2
                            anchors.rightMargin: NTheme.s2
                            spacing: NTheme.s2

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    text: cartRow.label
                                    elide: Text.ElideRight
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    font.weight: Font.DemiBold
                                    color: NTheme.textPrimary
                                }
                                // Prix unitaire éditable — négociation sur place (F04-01)
                                RowLayout {
                                    spacing: 4

                                    NTextField {
                                        Layout.preferredWidth: 74
                                        implicitHeight: 24
                                        fieldColor: NTheme.surfaceCard
                                        text: cartRow.unitPriceRaw
                                        font.pixelSize: NTheme.fontSizeSmall
                                        onEditingFinished:
                                            Sales.setLinePrice(cartRow.index, text)
                                    }
                                    Text {
                                        text: "DT"
                                        font.family: NTheme.fontFamily
                                        font.pixelSize: NTheme.fontSizeSmall
                                        color: NTheme.textSecondary
                                    }
                                    Text {
                                        visible: cartRow.manualPrice
                                        text: "✎"
                                        font.pixelSize: NTheme.fontSizeSmall
                                        color: NTheme.accent
                                    }
                                }
                            }

                            NIconButton {
                                text: "−"
                                variant: "secondary"
                                implicitHeight: 30
                                enabled: cartRow.qty > 1
                                onClicked: Sales.setQty(cartRow.index, cartRow.qty - 1)
                            }
                            Text {
                                text: cartRow.qty
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 24
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.Bold
                                color: NTheme.textPrimary
                            }
                            NIconButton {
                                text: "＋"
                                variant: "secondary"
                                implicitHeight: 30
                                onClicked: Sales.setQty(cartRow.index, cartRow.qty + 1)
                            }

                            Text {
                                Layout.preferredWidth: 76
                                text: cartRow.lineTotal
                                horizontalAlignment: Text.AlignRight
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                            NIconButton {
                                text: "✕"
                                implicitHeight: 28
                                onClicked: Sales.removeAt(cartRow.index)
                            }
                        }
                    }

                    NEmptyState {
                        anchors.centerIn: parent
                        visible: cartList.count === 0
                        emoji: "🛒"
                        message: qsTr("Panier vide — cherchez un produit à gauche.")
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

                // Remise globale (F04-02)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    NFieldLabel { text: qsTr("Remise (DT)") }
                    NTextField {
                        id: discountField
                        Layout.preferredWidth: 100
                        fieldColor: NTheme.surface
                        placeholderText: "0,000"
                        onEditingFinished: Sales.setGlobalDiscount(text)
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        visible: Sales.hasDiscount
                        text: Sales.discountDisplay
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.warning
                    }
                }

                // TOTAL (doc 03 : fontSizeTotal)
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("TOTAL")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeTitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: Sales.totalDisplay
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeTotal
                        font.weight: Font.Bold
                        color: NTheme.primaryDark
                    }
                }

                NButton {
                    Layout.fillWidth: true
                    large: true
                    text: qsTr("💵 PAYER  [F10]")
                    enabled: cartList.count > 0
                    onClicked: {
                        paymentDialog.clearForm();
                        paymentDialog.open();
                    }
                }
            }
        }
    }

    // ── Dialogue de paiement (F04-03, F04-04, F04-05) ─────────
    NDialog {
        id: paymentDialog

        property string method: "cash"

        function clearForm() {
            method = "cash";
            givenField.text = "";
            chequeNumberField.text = "";
            chequeBankField.text = "";
            errorText = "";
        }

        width: 440
        title: qsTr("Encaissement — %1").arg(Sales.totalDisplay)
        acceptText: qsTr("Encaisser")
        onOpened: givenField.forceActiveFocus()
        onAcceptClicked: Sales.checkout({
            method: paymentDialog.method,
            chequeNumber: chequeNumberField.text,
            chequeBank: chequeBankField.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s3

            // Client (choisi dans le panier, F04-04)
            RowLayout {
                spacing: NTheme.s2

                Text {
                    text: Sales.cartCustomerId > 0
                        ? "👤 " + Sales.cartCustomerName
                        : qsTr("👤 Client de passage")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: NTheme.textPrimary
                }
                NBadge {
                    visible: Sales.proPricing
                    text: qsTr("Prix pro")
                    badgeColor: NTheme.leaf
                }
            }

            // Mode de paiement
            RowLayout {
                spacing: NTheme.s2

                component MethodButton: NButton {
                    property string method
                    variant: paymentDialog.method === method ? "primary" : "secondary"
                    onClicked: paymentDialog.method = method
                }

                MethodButton { text: qsTr("💵 Espèces"); method: "cash" }
                MethodButton { text: qsTr("🧾 Chèque"); method: "cheque" }
                MethodButton { text: qsTr("🏦 Virement"); method: "transfer" }
                MethodButton {
                    text: qsTr("📒 Crédit")
                    method: "credit"
                    visible: Sales.cartCustomerId > 0
                }
            }

            Text {
                visible: paymentDialog.method === "credit"
                text: qsTr("La vente sera portée sur l'encours de %1.")
                    .arg(Sales.cartCustomerName)
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.warning
            }

            // Espèces : rendu monnaie en énorme (doc 03 §4.2)
            ColumnLayout {
                visible: paymentDialog.method === "cash"
                spacing: NTheme.s1

                NFieldLabel { text: qsTr("Montant donné (DT)") }
                NTextField {
                    id: givenField
                    Layout.preferredWidth: 160
                    fieldColor: NTheme.surface
                    font.pixelSize: NTheme.fontSizeTitle
                    placeholderText: "50"
                }
                RowLayout {
                    visible: givenField.text.length > 0
                            && Sales.changeFor(givenField.text).length > 0

                    Text {
                        text: qsTr("À rendre :")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        color: NTheme.textSecondary
                    }
                    Text {
                        text: Sales.changeFor(givenField.text)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeKpi
                        font.weight: Font.Bold
                        color: NTheme.primaryDark
                    }
                }
            }

            // Chèque
            ColumnLayout {
                visible: paymentDialog.method === "cheque"
                spacing: NTheme.s1

                NFieldLabel { text: qsTr("N° de chèque *") }
                NTextField {
                    id: chequeNumberField
                    Layout.fillWidth: true
                    fieldColor: NTheme.surface
                }
                NFieldLabel { text: qsTr("Banque") }
                NTextField {
                    id: chequeBankField
                    Layout.fillWidth: true
                    fieldColor: NTheme.surface
                }
            }
        }
    }

    // ── Journal du jour (F04-07) ──────────────────────────────
    NDialog {
        id: journalDialog

        property var rows: []
        property var totals: ({})

        width: 560
        title: qsTr("Journal des ventes — aujourd'hui")
        acceptText: qsTr("Fermer")
        cancelText: ""
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3

                NBadge {
                    text: qsTr("💵 %1").arg(journalDialog.totals.cash || "0,000 DT")
                    badgeColor: NTheme.primary
                }
                NBadge {
                    text: qsTr("🧾 %1").arg(journalDialog.totals.cheque || "0,000 DT")
                    badgeColor: NTheme.info
                }
                NBadge {
                    text: qsTr("🏦 %1").arg(journalDialog.totals.transfer || "0,000 DT")
                    badgeColor: NTheme.leaf
                }
            }

            ListView {
                Layout.fillWidth: true
                // Vide : place pour l'état « aucune vente » au lieu d'un filet
                Layout.preferredHeight: count === 0
                    ? 110 : Math.min(count, 9) * 40 + 8
                clip: true
                model: journalDialog.rows

                delegate: NListRow {
                    id: journalRow

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: 40
                    hoverable: false

                    Text {
                        text: journalRow.modelData.time
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        text: journalRow.modelData.number
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: journalRow.modelData.userName
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NBadge {
                        visible: journalRow.modelData.cancelled
                        text: qsTr("Annulée")
                        badgeColor: NTheme.danger
                    }
                    NBadge {
                        visible: journalRow.modelData.creditNote.length > 0
                        text: journalRow.modelData.creditNoteCount > 1
                            ? qsTr("Avoirs ×%1 · %2")
                                  .arg(journalRow.modelData.creditNoteCount)
                                  .arg(journalRow.modelData.creditNote)
                            : qsTr("Avoir %1").arg(journalRow.modelData.creditNote)
                        badgeColor: NTheme.warning
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: journalRow.modelData.total
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.Bold
                        font.strikeout: journalRow.modelData.cancelled
                        color: journalRow.modelData.cancelled
                            ? NTheme.textSecondary : NTheme.primary
                    }
                    NIconButton {
                        text: "🧾"
                        implicitHeight: 30
                        onClicked: {
                            const url = Sales.ticketFor(journalRow.modelData.id);
                            if (url.length > 0)
                                Qt.openUrlExternally(url);
                        }
                    }
                    // Facture PDF (M05) — pour les clients pros
                    NIconButton {
                        visible: !journalRow.modelData.cancelled
                        text: "📄"
                        implicitHeight: 30
                        onClicked: {
                            const url = Invoices.invoiceForSale(journalRow.modelData.id);
                            if (url.length > 0)
                                Qt.openUrlExternally(url);
                        }
                    }
                    // Annulation jour même — Gérant uniquement (F04-08)
                    NIconButton {
                        visible: !journalRow.modelData.cancelled
                                 && Auth.currentRole === "manager"
                        text: "✕"
                        implicitHeight: 30
                        onClicked: cancelDialog.openFor(journalRow.modelData)
                    }
                    // Avoir, partiel admis : visible tant qu'il reste du
                    // remboursable (norme : contre-passation, jamais de
                    // suppression)
                    NIconButton {
                        visible: journalRow.modelData.refundable
                                 && Auth.currentRole === "manager"
                        text: "↩"
                        implicitHeight: 30
                        onClicked: creditNoteDialog.openFor(journalRow.modelData)
                    }
                    // Réimpression du PDF de l'avoir existant
                    NIconButton {
                        visible: journalRow.modelData.creditNote.length > 0
                        text: "↩📄"
                        implicitHeight: 30
                        implicitWidth: 48
                        onClicked: {
                            const url = Sales.creditNotePdfFor(journalRow.modelData.id);
                            if (url.length > 0)
                                Qt.openUrlExternally(url);
                        }
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: journalDialog.rows.length === 0
                    emoji: "🧾"
                    message: qsTr("Aucune vente aujourd'hui.")
                }
            }
        }
    }

    // ── Annulation d'une vente (F04-08) ───────────────────────
    NDialog {
        id: cancelDialog

        property var sale: null

        function openFor(saleData) {
            sale = saleData;
            reasonField.text = "";
            errorText = "";
            open();
        }

        width: 420
        title: sale ? qsTr("Annuler la vente %1 ?").arg(sale.number) : ""
        acceptText: qsTr("Annuler la vente")
        cancelText: qsTr("Retour")
        acceptEnabled: reasonField.text.trim().length > 0
        onOpened: reasonField.forceActiveFocus()
        onAcceptClicked: Sales.cancelSale(sale.id, reasonField.text)

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Text {
                Layout.fillWidth: true
                text: qsTr("Le stock sera restitué et la vente marquée annulée.\n"
                           + "Possible le jour même uniquement.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textSecondary
            }

            NFieldLabel { text: qsTr("Motif *") }
            NTextField {
                id: reasonField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. erreur de saisie, client rétracté…")
            }
        }
    }

    // ── Avoir / note de crédit (norme comptable) ──────────────
    NDialog {
        id: creditNoteDialog

        property var sale: null
        // Avoir partiel : quantités à rembourser, éditables par ligne
        // (pré-remplies au restant = avoir total du reste).
        property var lines: []

        function openFor(saleData) {
            sale = saleData;
            lines = Sales.refundableLines(saleData.id).map(function (line) {
                line.qty = line.remaining;
                return line;
            });
            cnReasonField.text = "";
            cnRestock.checked = true;
            cnMethodBox.currentIndex = 0;
            errorText = "";
            open();
        }

        function anythingToRefund() {
            for (var i = 0; i < lines.length; ++i)
                if (lines[i].qty > 0)
                    return true;
            return false;
        }

        width: 520
        title: sale ? qsTr("Avoir sur la vente %1").arg(sale.number) : ""
        acceptText: qsTr("Émettre l'avoir")
        acceptEnabled: cnReasonField.text.trim().length > 0
                       && anythingToRefund()
        onOpened: cnReasonField.forceActiveFocus()
        onAcceptClicked: Sales.createCreditNote({
            saleId: sale.id,
            reason: cnReasonField.text,
            restock: cnRestock.checked,
            refundMethod: cnMethodBox.currentValue,
            locationId: cnLocationBox.currentValue,
            lines: lines.map(function (line) {
                return { saleLineId: line.saleLineId, qty: line.qty };
            }),
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Text {
                Layout.fillWidth: true
                text: qsTr("La vente n'est jamais supprimée : l'avoir %1 la "
                           + "contre-passe avec un document numéroté (norme).")
                    .arg("AV")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }

            // ── Quantités à rembourser (avoir partiel) ────────
            NFieldLabel { text: qsTr("Quantités à rembourser") }
            Repeater {
                model: creditNoteDialog.lines

                delegate: RowLayout {
                    id: cnLine

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: cnLine.modelData.label
                            + "  ·  " + cnLine.modelData.unitPriceDisplay
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        text: cnLine.modelData.qtyRefunded > 0
                            ? qsTr("%1/%2 déjà remboursé")
                                  .arg(cnLine.modelData.qtyRefunded)
                                  .arg(cnLine.modelData.qtySold)
                            : qsTr("vendu : %1").arg(cnLine.modelData.qtySold)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    NTextField {
                        Layout.preferredWidth: 64
                        fieldColor: NTheme.surface
                        horizontalAlignment: TextInput.AlignHCenter
                        enabled: cnLine.modelData.remaining > 0
                        text: String(cnLine.modelData.qty)
                        validator: IntValidator {
                            bottom: 0
                            top: cnLine.modelData.remaining
                        }
                        onEditingFinished: {
                            var updated = creditNoteDialog.lines.slice();
                            updated[cnLine.index].qty =
                                Math.min(parseInt(text) || 0,
                                         cnLine.modelData.remaining);
                            creditNoteDialog.lines = updated;
                        }
                    }
                }
            }

            NFieldLabel { text: qsTr("Motif *") }
            NTextField {
                id: cnReasonField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. saisie erronée détectée à la vérification")
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Remboursement") }
                    NComboBox {
                        id: cnMethodBox
                        Layout.preferredWidth: 190
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("💵 Espèces"), value: "cash" },
                            { label: qsTr("🧾 Chèque"), value: "cheque" },
                            { label: qsTr("🏦 Virement"), value: "transfer" },
                            { label: qsTr("📒 Sur l'encours client"), value: "credit" },
                        ]
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Marchandise") }
                    CheckBox {
                        id: cnRestock
                        checked: true
                        text: qsTr("Retour en stock")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                    }
                }
            }

            ColumnLayout {
                visible: cnRestock.checked
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Vers l'emplacement") }
                NComboBox {
                    id: cnLocationBox
                    Layout.preferredWidth: 220
                    textRole: "label"
                    valueRole: "id"
                    model: Stock.locationOptions()
                }
            }
        }
    }

    // ── Sélection du client du panier (F04-04, RG-04.c) ───────
    NDialog {
        id: customerDialog

        property var suggestions: []

        width: 420
        title: qsTr("Client de la vente")
        acceptText: qsTr("Fermer")
        cancelText: ""
        onOpened: { suggestions = []; customerSearchField.text = ""; customerSearchField.forceActiveFocus(); }
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Text {
                Layout.fillWidth: true
                text: qsTr("Un client professionnel bascule automatiquement le panier au tarif pro.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }

            NTextField {
                id: customerSearchField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("Nom ou téléphone (2 lettres min)…")
                onTextEdited: customerDialog.suggestions =
                    text.length >= 2 ? Sales.searchCustomers(text) : []
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: count === 0
                    ? (customerSearchField.text.length >= 2 ? 100 : 0)
                    : Math.min(count, 6) * 38
                clip: true
                model: customerDialog.suggestions

                delegate: Rectangle {
                    id: custRow
                    required property var modelData

                    width: ListView.view.width
                    height: 38
                    radius: NTheme.radiusButton
                    color: custHover.hovered ? NTheme.primaryContainer : "transparent"

                    HoverHandler { id: custHover }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: NTheme.s2
                        text: custRow.modelData.name
                            + (custRow.modelData.hasDebt
                               ? "  ·  " + qsTr("encours %1").arg(custRow.modelData.balanceDisplay)
                               : "")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }

                    TapHandler {
                        onTapped: {
                            Sales.setCartCustomer(custRow.modelData.id);
                            customerDialog.close();
                        }
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: customerDialog.suggestions.length === 0
                             && customerSearchField.text.length >= 2
                    emoji: "👤"
                    message: qsTr("Aucun client trouvé.")
                }
            }
        }
    }

    // ── Paniers en attente (F04-10) ───────────────────────────
    NDialog {
        id: heldDialog

        width: 440
        title: qsTr("Paniers en attente")
        acceptText: qsTr("Fermer")
        cancelText: ""
        onAcceptClicked: close()

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            Repeater {
                model: Sales.heldCarts

                delegate: NListRow {
                    id: heldRow
                    required property var modelData

                    Layout.fillWidth: true
                    height: 48
                    baseColor: NTheme.surface
                    showSeparator: false
                    onClicked: { Sales.resumeCart(heldRow.modelData.index); heldDialog.close(); }

                    Text { text: "🛒"; font.pixelSize: 18 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: heldRow.modelData.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            text: qsTr("%n article(s)", "", heldRow.modelData.itemCount)
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }
                    Text {
                        text: heldRow.modelData.total
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.Bold
                        color: NTheme.primary
                    }
                    NButton {
                        text: qsTr("Reprendre")
                        variant: "secondary"
                        onClicked: { Sales.resumeCart(heldRow.modelData.index); heldDialog.close(); }
                    }
                }
            }
        }
    }

    // ── Clôture de caisse (F04-09) ────────────────────────────
    NDialog {
        id: closureDialog

        property string expected: "0,000 DT"
        property alias countedText: countedField.text

        width: 400
        title: qsTr("Clôture de caisse")
        acceptText: qsTr("Valider la clôture")
        acceptEnabled: countedField.text.length > 0
        onOpened: countedField.forceActiveFocus()
        onAcceptClicked: Sales.recordClosure(countedField.text)

        contentItem: ColumnLayout {
            spacing: NTheme.s3

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Espèces théoriques du jour :")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    color: NTheme.textSecondary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: closureDialog.expected
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.primary
                }
            }

            ColumnLayout {
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Espèces comptées en caisse (DT) *") }
                NTextField {
                    id: countedField
                    Layout.preferredWidth: 180
                    fieldColor: NTheme.surface
                    font.pixelSize: NTheme.fontSizeTitle
                    placeholderText: "0,000"
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("L'écart éventuel (compté − théorique) sera journalisé.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }
}
