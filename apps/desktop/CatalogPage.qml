import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Nursera.UI
import Nursera.App

// Écran Catalogue (M02) : recherche multilingue, filtre catégorie,
// liste des produits, création rapide (F02-01, F02-06, F02-09).
Item {
    id: page

    Component.onCompleted: Catalog.refresh()

    Connections {
        target: Catalog

        function onProductCreated(productId) {
            addDialog.close();
            addDialog.clearForm();
        }
        function onProductSaved() {
            if (editDialog.opened)
                editDialog.reloadVariants();
        }
        function onVariantSaved() {
            variantDialog.close();
            editDialog.reloadVariants();
        }
        function onErrorOccurred(message) {
            if (variantDialog.opened)
                variantDialog.errorText = message;
            else if (editDialog.opened)
                editDialog.errorText = message;
            else if (addDialog.opened)
                addDialog.errorText = message;
        }
    }

    // Sélecteur d'image pour la photo produit (F02-01)
    FileDialog {
        id: photoDialog
        title: qsTr("Choisir une photo")
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp)")]
        onAccepted: {
            if (editDialog.productId > 0)
                Catalog.setProductPhoto(editDialog.productId, selectedFile);
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
                id: searchField

                Layout.preferredWidth: 320
                placeholderText: qsTr("Rechercher (nom FR/AR, latin, SKU)…")
                onTextEdited: Catalog.searchTerm = text
            }

            NComboBox {
                Layout.preferredWidth: 260
                model: [{ id: 0, label: qsTr("Toutes catégories") }]
                    .concat(Catalog.categoryOptions())
                textRole: "label"
                valueRole: "id"
                onActivated: Catalog.categoryFilter = currentValue
            }

            NButton {
                text: Catalog.showInactive ? qsTr("👁 Inactifs affichés")
                                           : qsTr("Voir inactifs")
                variant: "ghost"
                onClicked: Catalog.showInactive = !Catalog.showInactive
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("%n produit(s)", "", productList.count)
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textSecondary
            }

            NButton {
                text: qsTr("＋ Nouveau produit")
                onClicked: addDialog.open()
            }
        }

        // ── Liste des produits ────────────────────────────────
        NCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: productList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: Catalog.products
                boundsBehavior: Flickable.StopAtBounds

                delegate: NListRow {
                    id: row

                    required property int index
                    required property int productId
                    required property string nameFr
                    required property string nameAr
                    required property string botanicalName
                    required property string categoryFr
                    required property int variantCount
                    required property string minPrice
                    required property string photoUrl
                    required property bool isPlant
                    required property bool active

                    width: ListView.view.width
                    showSeparator: index < productList.count - 1
                    onClicked: editDialog.openFor(row.productId)

                    // Vignette photo (F02-01), sinon icône plante/article
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: NTheme.radiusButton
                        color: NTheme.surface
                        clip: true

                        Image {
                            anchors.fill: parent
                            visible: row.photoUrl.length > 0
                            source: row.photoUrl
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: row.photoUrl.length === 0
                            text: row.isPlant ? "🌱" : "🔧"
                            font.pixelSize: 22
                        }
                    }

                    ColumnLayout {
                        spacing: 2

                        RowLayout {
                            spacing: NTheme.s2

                            Text {
                                text: row.nameFr
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.DemiBold
                                color: NTheme.textPrimary
                            }
                            Text {
                                visible: row.nameAr.length > 0
                                text: row.nameAr
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textSecondary
                            }
                            NBadge {
                                visible: !row.active
                                text: qsTr("Inactif")
                                badgeColor: NTheme.danger
                            }
                        }
                        Text {
                            visible: row.botanicalName.length > 0
                            text: row.botanicalName
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            font.italic: true
                            color: NTheme.textSecondary
                        }
                    }

                    Item { Layout.fillWidth: true }

                    NBadge {
                        visible: row.categoryFr.length > 0
                        text: row.categoryFr
                        badgeColor: NTheme.leaf
                    }

                    Text {
                        text: qsTr("%n cond.", "", row.variantCount)
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }

                    Text {
                        visible: row.minPrice.length > 0
                        text: row.minPrice
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.primary
                    }
                }

                NEmptyState {
                    anchors.centerIn: parent
                    visible: productList.count === 0
                    emoji: "🪴"
                    message: searchField.text.length > 0
                        ? qsTr("Aucun produit ne correspond à la recherche.")
                        : qsTr("Le catalogue est vide — ajoutez votre première plante.")
                    actionText: searchField.text.length === 0
                        ? qsTr("＋ Ajouter un produit") : ""
                    onActionClicked: addDialog.open()
                }
            }
        }
    }

    // ── Dialogue de création (F02-01) ─────────────────────────
    NDialog {
        id: addDialog

        function clearForm() {
            nameFrField.text = "";
            nameArField.text = "";
            botanicalField.text = "";
            packagingField.text = "";
            priceField.text = "";
            errorText = "";
        }

        width: 460
        title: qsTr("Nouveau produit")
        acceptText: qsTr("Enregistrer")
        onOpened: nameFrField.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Catalog.createProduct({
            nameFr: nameFrField.text,
            nameAr: nameArField.text,
            botanicalName: botanicalField.text,
            categoryId: categoryBox.currentValue,
            type: typeBox.currentValue,
            packaging: packagingField.text,
            price: priceField.text,
            vatRate: vatBox.currentValue,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Nom commercial FR *") }
            NTextField {
                id: nameFrField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. Citronnier 4 saisons")
            }

            NFieldLabel { text: qsTr("Nom commercial AR") }
            NTextField {
                id: nameArField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: "ليمون الأربعة فصول"
            }

            NFieldLabel { text: qsTr("Nom botanique (latin)") }
            NTextField {
                id: botanicalField
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: "Citrus limon"
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Catégorie") }
                    NComboBox {
                        id: categoryBox
                        Layout.fillWidth: true
                        model: Catalog.categoryOptions()
                        textRole: "label"
                        valueRole: "id"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Nature") }
                    NComboBox {
                        id: typeBox
                        Layout.preferredWidth: 150
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("🌱 Plante"), value: "plant" },
                            { label: qsTr("🔧 Article"), value: "goods" },
                        ]
                    }
                }
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Conditionnement") }
                    NTextField {
                        id: packagingField
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        placeholderText: typeBox.currentValue === "goods"
                            ? qsTr("unité") : qsTr("godet")
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Prix TTC (DT) *") }
                    NTextField {
                        id: priceField
                        Layout.preferredWidth: 110
                        fieldColor: NTheme.surface
                        placeholderText: "12,500"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("TVA") }
                    NComboBox {
                        id: vatBox
                        Layout.preferredWidth: 90
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: "0 %", value: 0 }, { label: "7 %", value: 7 },
                            { label: "13 %", value: 13 }, { label: "19 %", value: 19 },
                        ]
                    }
                }
            }
        }
    }

    // ── Fiche produit : édition + conditionnements (F02) ──────
    NDialog {
        id: editDialog

        property int productId: 0
        property bool productActive: true
        property bool deletable: false
        property bool confirmDelete: false
        property var variants: []
        property string photoUrl: ""

        function openFor(id) {
            errorText = "";
            const p = Catalog.productDetail(id);
            productId = id;
            productActive = p.active !== false;
            deletable = Catalog.isDeletable(id);
            confirmDelete = false;
            eNameFr.text = p.nameFr || "";
            eNameAr.text = p.nameAr || "";
            eBotanical.text = p.botanicalName || "";
            eCategory.currentIndex = Math.max(0,
                eCategory.indexOfValue(p.categoryId));
            eType.currentIndex = p.type === "goods" ? 1 : 0;
            photoUrl = Catalog.photoUrl(id);
            reloadVariants();
            open();
        }

        function reloadVariants() {
            variants = Catalog.variantsOf(productId);
            photoUrl = Catalog.photoUrl(productId);
        }

        width: 640
        title: qsTr("Fiche produit")
        acceptText: qsTr("Enregistrer")
        onAcceptClicked: Catalog.saveProduct({
            id: editDialog.productId,
            nameFr: eNameFr.text,
            nameAr: eNameAr.text,
            botanicalName: eBotanical.text,
            categoryId: eCategory.currentValue,
            type: eType.currentValue,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            // Photo (F02-01) + champs nom, côte à côte
            RowLayout {
                spacing: NTheme.s3

                Rectangle {
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 96
                    radius: NTheme.radiusCard
                    color: NTheme.surface
                    border.width: 1
                    border.color: NTheme.outline
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        visible: editDialog.photoUrl.length > 0
                        source: editDialog.photoUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: editDialog.photoUrl.length === 0
                        text: "📷"
                        font.pixelSize: 30
                        opacity: 0.4
                    }
                    TapHandler { onTapped: photoDialog.open() }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    RowLayout {
                        spacing: NTheme.s3
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: NTheme.s1
                            NFieldLabel { text: qsTr("Nom FR *") }
                            NTextField { id: eNameFr; Layout.fillWidth: true; fieldColor: NTheme.surface }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: NTheme.s1
                            NFieldLabel { text: qsTr("Nom AR") }
                            NTextField { id: eNameAr; Layout.fillWidth: true; fieldColor: NTheme.surface }
                        }
                    }
                    NButton {
                        text: editDialog.photoUrl.length > 0
                            ? qsTr("📷 Changer la photo") : qsTr("📷 Ajouter une photo")
                        variant: "secondary"
                        onClicked: photoDialog.open()
                    }
                }
            }

            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Nom botanique") }
                    NTextField { id: eBotanical; Layout.fillWidth: true; fieldColor: NTheme.surface }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Catégorie") }
                    NComboBox {
                        id: eCategory
                        Layout.preferredWidth: 200
                        model: Catalog.categoryOptions()
                        textRole: "label"
                        valueRole: "id"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Nature") }
                    NComboBox {
                        id: eType
                        Layout.preferredWidth: 130
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("🌱 Plante"), value: "plant" },
                            { label: qsTr("🔧 Article"), value: "goods" },
                        ]
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: NTheme.outline }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Conditionnements")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
                Item { Layout.fillWidth: true }
                NButton {
                    text: qsTr("🏷 Étiquettes QR")
                    variant: "ghost"
                    onClicked: {
                        const url = Catalog.printLabels(editDialog.productId);
                        if (url.length > 0)
                            Qt.openUrlExternally(url);
                    }
                }
                NButton {
                    text: qsTr("＋ Ajouter")
                    variant: "secondary"
                    onClicked: variantDialog.openNew(editDialog.productId)
                }
            }

            Repeater {
                model: editDialog.variants

                delegate: NListRow {
                    id: vRow

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    height: 46
                    baseColor: NTheme.surface
                    showSeparator: false
                    onClicked: variantDialog.openEdit(editDialog.productId,
                                                      vRow.modelData)

                    NBadge { text: vRow.modelData.packaging; badgeColor: NTheme.info }
                    Text {
                        text: vRow.modelData.sku
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Text {
                        visible: vRow.modelData.barcode.length > 0
                        text: "▮▏ " + vRow.modelData.barcode
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: vRow.modelData.price + " DT"
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.Bold
                        color: NTheme.primary
                    }
                    Text {
                        text: vRow.modelData.vatRate + " %"
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                }
            }

            // Désactivation réversible ; suppression physique UNIQUEMENT si
            // aucun historique (ajout fautif — norme).
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: NTheme.s2

                NButton {
                    text: editDialog.productActive
                        ? qsTr("🚫 Désactiver")
                        : qsTr("↩ Réactiver")
                    variant: editDialog.productActive ? "ghost" : "secondary"
                    onClicked: {
                        if (Catalog.setProductActive(editDialog.productId,
                                                     !editDialog.productActive))
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
                        } else if (Catalog.deleteProduct(editDialog.productId)) {
                            editDialog.close();
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: editDialog.deletable
                        ? qsTr("Aucun historique : suppression possible.")
                        : qsTr("Historique existant : désactivation seulement.")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }
        }
    }

    // ── Conditionnement : ajout / édition ─────────────────────
    NDialog {
        id: variantDialog

        property int productId: 0
        property int variantId: 0

        function openNew(pid) {
            errorText = "";
            productId = pid; variantId = 0;
            vPackaging.text = ""; vPrice.text = ""; vPricePro.text = "";
            vBarcode.text = ""; vAlert.text = ""; vVat.currentIndex = 0;
            open();
        }
        function openEdit(pid, data) {
            errorText = "";
            productId = pid; variantId = data.id;
            vPackaging.text = data.packaging;
            vPrice.text = data.price;
            vPricePro.text = data.pricePro;
            vBarcode.text = data.barcode;
            vAlert.text = data.alertThreshold >= 0 ? String(data.alertThreshold) : "";
            vVat.currentIndex = vVat.indexOfValue(data.vatRate);
            open();
        }

        width: 440
        title: variantId > 0 ? qsTr("Modifier le conditionnement")
                             : qsTr("Nouveau conditionnement")
        acceptText: qsTr("Enregistrer")
        onAcceptClicked: Catalog.saveVariant({
            id: variantDialog.variantId,
            productId: variantDialog.productId,
            packaging: vPackaging.text,
            price: vPrice.text,
            pricePro: vPricePro.text,
            barcode: vBarcode.text,
            vatRate: vVat.currentValue,
            alertThreshold: vAlert.text.length > 0 ? parseInt(vAlert.text) : -1,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Conditionnement *") }
                    NTextField {
                        id: vPackaging
                        Layout.fillWidth: true
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("pot14, unité, sac 50L…")
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("TVA") }
                    NComboBox {
                        id: vVat
                        Layout.preferredWidth: 90
                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: "0 %", value: 0 }, { label: "7 %", value: 7 },
                            { label: "13 %", value: 13 }, { label: "19 %", value: 19 },
                        ]
                    }
                }
            }

            RowLayout {
                spacing: NTheme.s3
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Prix TTC (DT) *") }
                    NTextField {
                        id: vPrice
                        Layout.preferredWidth: 120
                        fieldColor: NTheme.surface
                        placeholderText: "25,000"
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Prix pro (DT)") }
                    NTextField {
                        id: vPricePro
                        Layout.preferredWidth: 120
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("optionnel")
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Seuil alerte") }
                    NTextField {
                        id: vAlert
                        Layout.preferredWidth: 90
                        fieldColor: NTheme.surface
                        placeholderText: qsTr("aucun")
                        validator: IntValidator { bottom: 0 }
                    }
                }
            }

            NFieldLabel { text: qsTr("Code-barres (scan caisse)") }
            NTextField {
                id: vBarcode
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("EAN / code produit")
            }
        }
    }
}
