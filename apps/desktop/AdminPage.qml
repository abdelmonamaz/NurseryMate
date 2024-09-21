import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Réglages (M11) : société (F11-01), utilisateurs (F11-05 partiel),
// sauvegardes (F11-04). Gérant uniquement.
Item {
    id: page

    property string banner: ""

    Component.onCompleted: Admin.refresh()

    Connections {
        target: Admin

        function onCompanySaved() { page.banner = qsTr("✅ Coordonnées enregistrées — elles apparaîtront sur les prochains tickets."); }
        function onBackupDone(fileName) { page.banner = qsTr("✅ Sauvegarde créée : %1").arg(fileName); }
        function onUserSaved() {
            page.banner = qsTr("✅ Utilisateurs mis à jour.");
            userDialog.close();
            userDialog.clearForm();
            pinDialog.close();
            Auth.refresh();
        }
        function onReferentialSaved() {
            page.banner = qsTr("✅ Référentiels mis à jour.");
            categoryDialog.close();
            locationDialog.close();
        }
        function onErrorOccurred(message) {
            if (userDialog.opened)
                userDialog.errorText = message;
            else if (pinDialog.opened)
                pinDialog.errorText = message;
            else if (categoryDialog.opened)
                categoryDialog.errorText = message;
            else if (locationDialog.opened)
                locationDialog.errorText = message;
            else if (restoreDialog.opened)
                restoreDialog.errorText = message;
            else
                page.banner = "⚠ " + message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        spacing: NTheme.s3

        NCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            visible: page.banner.length > 0
            color: NTheme.primaryContainer
            border.color: NTheme.leaf

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: NTheme.s3
                text: page.banner
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textPrimary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: NTheme.s3

            // ══ Colonne gauche : société + sauvegardes + numéros ══
            // Défilante : la colonne a grandi (sync, QR, restauration…)
            Flickable {
                Layout.fillWidth: false
                Layout.preferredWidth: 420
                Layout.maximumWidth: 420
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: leftColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: leftColumn
                width: parent.width
                spacing: NTheme.s3

                // ── Société (F11-01) ──────────────────────────
                NCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: companyColumn.implicitHeight + NTheme.s4 * 2

                    ColumnLayout {
                        id: companyColumn

                        anchors.fill: parent
                        anchors.margins: NTheme.s4
                        spacing: NTheme.s2

                        Text {
                            text: qsTr("🏢 Coordonnées de l'entreprise")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: NTheme.textPrimary
                        }

                        NFieldLabel { text: qsTr("Nom") }
                        NTextField {
                            id: companyName
                            Layout.fillWidth: true
                            fieldColor: NTheme.surface
                            text: Admin.company.name || ""
                        }
                        NFieldLabel { text: qsTr("Slogan") }
                        NTextField {
                            id: companyTagline
                            Layout.fillWidth: true
                            fieldColor: NTheme.surface
                            text: Admin.company.tagline || ""
                        }
                        NFieldLabel { text: qsTr("Adresse") }
                        NTextField {
                            id: companyAddress
                            Layout.fillWidth: true
                            fieldColor: NTheme.surface
                            text: Admin.company.address || ""
                            placeholderText: qsTr("Route de Tunis, Sfax")
                        }
                        RowLayout {
                            spacing: NTheme.s3

                            ColumnLayout {
                                spacing: NTheme.s1
                                NFieldLabel { text: qsTr("Téléphone") }
                                NTextField {
                                    id: companyPhone
                                    Layout.preferredWidth: 170
                                    fieldColor: NTheme.surface
                                    text: Admin.company.phone || ""
                                }
                            }
                            ColumnLayout {
                                spacing: NTheme.s1
                                NFieldLabel { text: qsTr("Matricule fiscal") }
                                NTextField {
                                    id: companyTaxId
                                    Layout.preferredWidth: 170
                                    fieldColor: NTheme.surface
                                    text: Admin.company.tax_id || ""
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: NTheme.s3

                            ColumnLayout {
                                spacing: NTheme.s1
                                NFieldLabel { text: qsTr("Timbre fiscal (DT)") }
                                NTextField {
                                    id: stampDuty
                                    Layout.preferredWidth: 120
                                    fieldColor: NTheme.surface
                                    text: Admin.stampDuty
                                    placeholderText: "1,000"
                                }
                            }
                            Item { Layout.fillWidth: true }
                            NButton {
                                Layout.alignment: Qt.AlignBottom
                                text: qsTr("Enregistrer")
                                onClicked: {
                                    Admin.saveStampDuty(stampDuty.text);
                                    Admin.saveCompany({
                                        name: companyName.text,
                                        tagline: companyTagline.text,
                                        address: companyAddress.text,
                                        phone: companyPhone.text,
                                        tax_id: companyTaxId.text,
                                    });
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: NTheme.outline
                        }

                        // Sync mobile (M09) : ce poste héberge le serveur LAN
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: NTheme.s2

                            CheckBox {
                                id: syncCheck
                                checked: Admin.syncEnabled
                                text: qsTr("📡 Poste principal (sync mobile)")
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                onToggled: Admin.saveSyncSettings(
                                    checked, syncPortField.text)
                            }
                            Item { Layout.fillWidth: true }
                            NFieldLabel { text: qsTr("Port") }
                            NTextField {
                                id: syncPortField
                                Layout.preferredWidth: 76
                                fieldColor: NTheme.surface
                                text: Admin.syncPort
                                validator: IntValidator { bottom: 1; top: 65535 }
                                onEditingFinished:
                                    Admin.saveSyncSettings(syncCheck.checked, text)
                            }
                        }
                        RowLayout {
                            visible: Admin.syncEnabled
                            Layout.fillWidth: true
                            spacing: NTheme.s3

                            // QR d'appairage (F09-09) : nursera://IP:PORT
                            // 220px ≈ résolution native de l'image générée
                            // (scale 6) — un affichage plus petit la
                            // sous-échantillonnait au point de la rendre
                            // illisible par une caméra de téléphone.
                            Image {
                                Layout.preferredWidth: 220
                                Layout.preferredHeight: 220
                                fillMode: Image.PreserveAspectFit
                                smooth: false // modules nets
                                source: Admin.syncEnabled
                                    ? Admin.syncQrUrl() : ""
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Sur le mobile : scannez ce code ou saisissez l'IP %1 — appliqué au prochain démarrage.")
                                    .arg(Admin.syncAddresses)
                                wrapMode: Text.WordWrap
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.textSecondary
                            }
                        }
                    }
                }

                // ── Sauvegardes (F11-04) ──────────────────────
                NCard {
                    Layout.fillWidth: true
                    // Hauteur fixe : la colonne défile désormais.
                    Layout.preferredHeight: 250

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: NTheme.s4
                        spacing: NTheme.s2

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: qsTr("💾 Sauvegardes")
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.Bold
                                color: NTheme.textPrimary
                            }
                            Item { Layout.fillWidth: true }
                            NButton {
                                text: qsTr("📂 Ouvrir")
                                variant: "ghost"
                                onClicked: Qt.openUrlExternally(Admin.backupDirUrl)
                            }
                            NButton {
                                text: qsTr("Sauvegarder")
                                onClicked: Admin.backupNow()
                            }
                        }

                        Text {
                            text: qsTr("Automatique une fois par jour au démarrage · conservées 30 jours.")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }

                        // Restauration en attente (F11-04) — appliquée au
                        // prochain démarrage, annulable d'ici là.
                        NCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: pendingRow.implicitHeight + NTheme.s2 * 2
                            visible: Admin.restorePending
                            color: Qt.alpha(NTheme.warning, 0.12)
                            border.color: NTheme.warning

                            RowLayout {
                                id: pendingRow
                                anchors.fill: parent
                                anchors.leftMargin: NTheme.s3
                                anchors.rightMargin: NTheme.s2
                                spacing: NTheme.s2

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("⏳ Restauration au prochain démarrage — fermez puis rouvrez l'application.")
                                    wrapMode: Text.WordWrap
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textPrimary
                                }
                                NButton {
                                    text: qsTr("Annuler")
                                    variant: "ghost"
                                    onClicked: Admin.cancelRestore()
                                }
                                NButton {
                                    text: qsTr("Quitter")
                                    variant: "secondary"
                                    onClicked: Qt.quit()
                                }
                            }
                        }

                        ListView {
                            id: backupList

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: Admin.backups
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: NListRow {
                                id: backupRow

                                required property int index
                                required property var modelData

                                width: ListView.view.width
                                height: 36
                                hoverable: false
                                showSeparator: index < backupList.count - 1

                                Text {
                                    Layout.fillWidth: true
                                    text: backupRow.modelData.fileName
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textPrimary
                                }
                                Text {
                                    text: backupRow.modelData.size
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                                // Restauration guidée (F11-04)
                                NIconButton {
                                    text: "↩"
                                    implicitHeight: 28
                                    onClicked: restoreDialog.openFor(
                                        backupRow.modelData.fileName)
                                }
                            }

                            NEmptyState {
                                anchors.centerIn: parent
                                visible: backupList.count === 0
                                emoji: "💾"
                                message: qsTr("Aucune sauvegarde encore.")
                            }
                        }
                    }
                }

                // ── Numérotations documentaires (F11-07) ──────
                NCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: numbersColumn.implicitHeight + NTheme.s4 * 2

                    ColumnLayout {
                        id: numbersColumn

                        anchors.fill: parent
                        anchors.margins: NTheme.s4
                        spacing: NTheme.s1

                        Text {
                            text: qsTr("🔢 Prochains numéros")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: NTheme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Numérotation sans trou — informative, jamais modifiable.")
                            wrapMode: Text.WordWrap
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }

                        // 2 colonnes : la carte reste compacte sous les
                        // Sauvegardes (l'écran Réglages ne défile pas).
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: NTheme.s5
                            rowSpacing: NTheme.s1

                            Repeater {
                                model: Admin.docNumbers
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        elide: Text.ElideRight
                                        font.family: NTheme.fontFamily
                                        font.pixelSize: NTheme.fontSizeSmall
                                        color: NTheme.textPrimary
                                    }
                                    Text {
                                        text: modelData.number
                                        font.family: NTheme.fontFamily
                                        font.pixelSize: NTheme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        color: NTheme.primaryDark
                                    }
                                }
                            }
                        }
                    }
                }
            }
            } // Flickable colonne gauche

            // ══ Colonne droite : utilisateurs + référentiels ══
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: NTheme.s3

            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 5

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s4
                    spacing: NTheme.s2

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("👥 Utilisateurs")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: NTheme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        NButton {
                            text: qsTr("＋ Nouvel utilisateur")
                            onClicked: userDialog.open()
                        }
                    }

                    ListView {
                        id: userList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Admin.users
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: userRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 52
                            hoverable: false
                            showSeparator: index < userList.count - 1

                            Text { text: "👤"; font.pixelSize: 18 }
                            Text {
                                text: userRow.modelData.name
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: NTheme.textPrimary
                            }
                            NBadge {
                                text: userRow.modelData.role === "manager"
                                    ? qsTr("Gérant")
                                    : userRow.modelData.role === "seller"
                                      ? qsTr("Vendeur") : qsTr("Ouvrier")
                                badgeColor: userRow.modelData.role === "manager"
                                    ? NTheme.primary : NTheme.info
                            }
                            NBadge {
                                visible: !userRow.modelData.active
                                text: qsTr("Inactif")
                                badgeColor: NTheme.danger
                            }
                            Item { Layout.fillWidth: true }
                            NButton {
                                visible: userRow.modelData.active
                                text: qsTr("PIN")
                                variant: "secondary"
                                onClicked: pinDialog.openFor(userRow.modelData)
                            }
                            NButton {
                                visible: userRow.modelData.active
                                text: qsTr("Désactiver")
                                variant: "ghost"
                                onClicked: Admin.deactivateUser(userRow.modelData.id)
                            }
                            NButton {
                                visible: !userRow.modelData.active
                                text: qsTr("↩ Réactiver")
                                variant: "secondary"
                                onClicked: Admin.reactivateUser(userRow.modelData.id)
                            }
                        }
                    }
                }
            }

            // ── Référentiels (F11-06) : catégories + emplacements ──
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 4
                spacing: NTheme.s3

                NCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: NTheme.s4
                        spacing: NTheme.s2

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("🏷 Catégories")
                                elide: Text.ElideRight
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.Bold
                                color: NTheme.textPrimary
                            }
                            NButton {
                                text: "＋"
                                implicitWidth: 52
                                onClicked: categoryDialog.openFor(null)
                            }
                        }

                        ListView {
                            id: categoryList

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: Admin.categories
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: NListRow {
                                id: categoryRow

                                required property int index
                                required property var modelData

                                width: ListView.view.width
                                height: 40
                                showSeparator: index < categoryList.count - 1
                                onClicked: categoryDialog.openFor(categoryRow.modelData)

                                Text {
                                    Layout.fillWidth: true
                                    text: categoryRow.modelData.nameFr
                                          + (categoryRow.modelData.nameAr.length > 0
                                             ? " · " + categoryRow.modelData.nameAr : "")
                                    elide: Text.ElideRight
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    color: NTheme.textPrimary
                                }
                                NBadge {
                                    visible: !categoryRow.modelData.active
                                    text: qsTr("Inactive")
                                    badgeColor: NTheme.danger
                                }
                            }
                        }
                    }
                }

                NCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: NTheme.s4
                        spacing: NTheme.s2

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("📍 Emplacements")
                                elide: Text.ElideRight
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.Bold
                                color: NTheme.textPrimary
                            }
                            NButton {
                                text: "＋"
                                implicitWidth: 52
                                onClicked: locationDialog.openFor(null)
                            }
                        }

                        ListView {
                            id: locationList

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: Admin.locations
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: NListRow {
                                id: locationRow

                                required property int index
                                required property var modelData

                                width: ListView.view.width
                                height: 40
                                showSeparator: index < locationList.count - 1
                                onClicked: locationDialog.openFor(locationRow.modelData)

                                Text {
                                    Layout.fillWidth: true
                                    text: locationRow.modelData.nameFr
                                    elide: Text.ElideRight
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    color: NTheme.textPrimary
                                }
                                NBadge {
                                    text: locationRow.modelData.kindLabel
                                    badgeColor: NTheme.info
                                }
                                NBadge {
                                    visible: !locationRow.modelData.active
                                    text: qsTr("Inactif")
                                    badgeColor: NTheme.danger
                                }
                            }
                        }
                    }
                }
            }
            }
        }
    }

    // ── Nouvel utilisateur (F01-02) ───────────────────────────
    NDialog {
        id: userDialog

        function clearForm() {
            userName.text = "";
            userPin.text = "";
            userConfirm.text = "";
            roleBox.currentIndex = 2;
            errorText = "";
        }

        width: 400
        title: qsTr("Nouvel utilisateur")
        acceptText: qsTr("Créer")
        onOpened: userName.forceActiveFocus()
        onCancelClicked: clearForm()
        onAcceptClicked: Admin.createUser({
            name: userName.text,
            role: roleBox.currentValue,
            pin: userPin.text,
            confirm: userConfirm.text,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Nom *") }
            NTextField {
                id: userName
                Layout.fillWidth: true
                fieldColor: NTheme.surface
            }
            NFieldLabel { text: qsTr("Rôle") }
            NComboBox {
                id: roleBox
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                currentIndex: 2
                model: [
                    { label: qsTr("Gérant"), value: "manager" },
                    { label: qsTr("Vendeur"), value: "seller" },
                    { label: qsTr("Ouvrier"), value: "worker" },
                ]
            }
            RowLayout {
                spacing: NTheme.s3

                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("PIN (4-6 chiffres) *") }
                    NTextField {
                        id: userPin
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        echoMode: TextInput.Password
                        validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    }
                }
                ColumnLayout {
                    spacing: NTheme.s1
                    NFieldLabel { text: qsTr("Confirmer *") }
                    NTextField {
                        id: userConfirm
                        Layout.preferredWidth: 150
                        fieldColor: NTheme.surface
                        echoMode: TextInput.Password
                        validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    }
                }
            }
        }
    }

    // ── Réinitialiser le PIN ──────────────────────────────────
    NDialog {
        id: pinDialog

        property var user: null

        function openFor(userData) {
            user = userData;
            newPin.text = "";
            newConfirm.text = "";
            errorText = "";
            open();
        }

        width: 380
        title: user ? qsTr("Nouveau PIN — %1").arg(user.name) : ""
        acceptText: qsTr("Changer le PIN")
        onOpened: newPin.forceActiveFocus()
        onAcceptClicked: Admin.resetPin(user.id, newPin.text, newConfirm.text)

        contentItem: RowLayout {
            spacing: NTheme.s3

            ColumnLayout {
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Nouveau PIN *") }
                NTextField {
                    id: newPin
                    Layout.preferredWidth: 140
                    fieldColor: NTheme.surface
                    echoMode: TextInput.Password
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                }
            }
            ColumnLayout {
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Confirmer *") }
                NTextField {
                    id: newConfirm
                    Layout.preferredWidth: 140
                    fieldColor: NTheme.surface
                    echoMode: TextInput.Password
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                }
            }
        }
    }

    // ── Catégorie : création / édition (F11-06) ───────────────
    NDialog {
        id: categoryDialog

        property var category: null // null = création
        property bool deletable: false
        property bool confirmDelete: false

        function openFor(data) {
            category = data;
            errorText = "";
            confirmDelete = false;
            deletable = data ? Admin.categoryDeletable(data.id) : false;
            catNameFr.text = data ? data.nameFr : "";
            catNameAr.text = data ? data.nameAr : "";
            catSort.text = data ? String(data.sortOrder) : "0";
            open();
        }

        width: 420
        title: category ? qsTr("Catégorie — %1").arg(category.nameFr)
                        : qsTr("Nouvelle catégorie")
        acceptText: category ? qsTr("Enregistrer") : qsTr("Créer")
        acceptEnabled: catNameFr.text.trim().length > 0
        onOpened: catNameFr.forceActiveFocus()
        onAcceptClicked: Admin.saveCategory({
            id: category ? category.id : 0,
            nameFr: catNameFr.text,
            nameAr: catNameAr.text,
            sortOrder: parseInt(catSort.text) || 0,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Nom (FR) *") }
            NTextField {
                id: catNameFr
                Layout.fillWidth: true
                fieldColor: NTheme.surface
            }
            NFieldLabel { text: qsTr("Nom (AR)") }
            NTextField {
                id: catNameAr
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                horizontalAlignment: TextInput.AlignRight
            }
            NFieldLabel { text: qsTr("Ordre d'affichage") }
            NTextField {
                id: catSort
                Layout.preferredWidth: 100
                fieldColor: NTheme.surface
                validator: IntValidator { bottom: 0; top: 999 }
            }

            RowLayout {
                visible: categoryDialog.category !== null
                spacing: NTheme.s2

                NButton {
                    text: categoryDialog.category
                          && categoryDialog.category.active
                        ? qsTr("Désactiver") : qsTr("↩ Réactiver")
                    variant: "ghost"
                    onClicked: Admin.setCategoryActive(
                        categoryDialog.category.id,
                        !categoryDialog.category.active)
                }
                NButton {
                    visible: categoryDialog.deletable
                    text: categoryDialog.confirmDelete
                        ? qsTr("⚠ Confirmer la suppression ?")
                        : qsTr("🗑 Supprimer (jamais utilisée)")
                    variant: "ghost"
                    onClicked: {
                        if (!categoryDialog.confirmDelete)
                            categoryDialog.confirmDelete = true;
                        else
                            Admin.deleteCategory(categoryDialog.category.id);
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: categoryDialog.category !== null
                         && !categoryDialog.deletable
                Layout.fillWidth: true
                text: qsTr("Utilisée par des produits ou sous-catégories — désactivable, mais pas supprimable.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }

    // ── Emplacement : création / édition (F11-06) ─────────────
    NDialog {
        id: locationDialog

        property var location: null // null = création
        property bool deletable: false
        property bool confirmDelete: false

        function openFor(data) {
            location = data;
            errorText = "";
            confirmDelete = false;
            deletable = data ? Admin.locationDeletable(data.id) : false;
            locNameFr.text = data ? data.nameFr : "";
            locNameAr.text = data ? data.nameAr : "";
            locKind.currentIndex = data
                ? Math.max(0, locKind.indexOfValue(data.kind)) : 0;
            open();
        }

        width: 420
        title: location ? qsTr("Emplacement — %1").arg(location.nameFr)
                        : qsTr("Nouvel emplacement")
        acceptText: location ? qsTr("Enregistrer") : qsTr("Créer")
        acceptEnabled: locNameFr.text.trim().length > 0
        onOpened: locNameFr.forceActiveFocus()
        onAcceptClicked: Admin.saveLocation({
            id: location ? location.id : 0,
            nameFr: locNameFr.text,
            nameAr: locNameAr.text,
            kind: locKind.currentValue,
        })

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Nom (FR) *") }
            NTextField {
                id: locNameFr
                Layout.fillWidth: true
                fieldColor: NTheme.surface
            }
            NFieldLabel { text: qsTr("Nom (AR)") }
            NTextField {
                id: locNameAr
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                horizontalAlignment: TextInput.AlignRight
            }
            NFieldLabel { text: qsTr("Type") }
            NComboBox {
                id: locKind
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("Serre"), value: "greenhouse" },
                    { label: qsTr("Parcelle"), value: "field" },
                    { label: qsTr("Zone de vente"), value: "sales_area" },
                    { label: qsTr("Dépôt"), value: "warehouse" },
                ]
            }

            RowLayout {
                visible: locationDialog.location !== null
                spacing: NTheme.s2

                NButton {
                    text: locationDialog.location
                          && locationDialog.location.active
                        ? qsTr("Désactiver") : qsTr("↩ Réactiver")
                    variant: "ghost"
                    onClicked: Admin.setLocationActive(
                        locationDialog.location.id,
                        !locationDialog.location.active)
                }
                NButton {
                    visible: locationDialog.deletable
                    text: locationDialog.confirmDelete
                        ? qsTr("⚠ Confirmer la suppression ?")
                        : qsTr("🗑 Supprimer (jamais utilisé)")
                    variant: "ghost"
                    onClicked: {
                        if (!locationDialog.confirmDelete)
                            locationDialog.confirmDelete = true;
                        else
                            Admin.deleteLocation(locationDialog.location.id);
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                visible: locationDialog.location !== null
                         && !locationDialog.deletable
                Layout.fillWidth: true
                text: qsTr("Des mouvements, inventaires ou lots y font référence — désactivable, mais pas supprimable.")
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }

    // ── Restauration guidée d'une sauvegarde (F11-04) ─────────
    NDialog {
        id: restoreDialog

        property string fileName: ""

        function openFor(name) {
            fileName = name;
            errorText = "";
            open();
        }

        width: 460
        title: qsTr("Restaurer %1 ?").arg(fileName)
        acceptText: qsTr("Restaurer au prochain démarrage")
        onAcceptClicked: {
            if (Admin.stageRestore(fileName)) {
                close();
                page.banner = qsTr("⏳ Restauration prête — fermez puis rouvrez l'application pour l'appliquer.");
            }
        }

        contentItem: Text {
            Layout.fillWidth: true
            text: qsTr("La base actuelle sera remplacée par cette sauvegarde "
                       + "au prochain démarrage de l'application.\n\n"
                       + "Tout ce qui a été saisi APRÈS cette sauvegarde disparaîtra "
                       + "de la base restaurée. Par sécurité, la base actuelle est "
                       + "conservée en pre-restore-….db à côté de la base.")
            wrapMode: Text.WordWrap
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            color: NTheme.textPrimary
        }
    }
}
