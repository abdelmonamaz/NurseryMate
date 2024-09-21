import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Connexion par sélection d'utilisateur + PIN (F01-01) et création du
// compte Gérant au premier lancement (RG-01.a).
Rectangle {
    id: page

    property int selectedUserId: 0
    property string errorText: ""

    color: NTheme.surface

    Component.onCompleted: Auth.refresh()

    // Réinitialisation à chaque affichage (déconnexion incluse)
    onVisibleChanged: {
        if (visible) {
            selectedUserId = 0;
            errorText = "";
            pinField.text = "";
        }
    }

    Connections {
        target: Auth

        function onLoginFailed(message) { page.errorText = message; }
        function onErrorOccurred(message) { page.errorText = message; }
    }

    // Bloque les clics vers le contenu en dessous
    MouseArea { anchors.fill: parent }

    NCard {
        anchors.centerIn: parent
        width: 460
        height: content.implicitHeight + NTheme.s5 * 2

        ColumnLayout {
            id: content

            anchors.centerIn: parent
            width: parent.width - NTheme.s5 * 2
            spacing: NTheme.s4

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 110
                Layout.preferredHeight: 110
                source: "qrc:/resources/logo.jpeg"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: Auth.needsSetup ? qsTr("Bienvenue !")
                                      : qsTr("Qui êtes-vous ?")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeTitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }

            // ── Premier lancement : créer le compte Gérant ────
            ColumnLayout {
                visible: Auth.needsSetup
                Layout.fillWidth: true
                spacing: NTheme.s2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Créez le compte Gérant pour commencer.")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeBody
                    color: NTheme.textSecondary
                    wrapMode: Text.WordWrap
                }

                component LoginField: NTextField {
                    Layout.fillWidth: true
                    font.pixelSize: NTheme.fontSizeSubtitle
                }

                LoginField {
                    id: nameField
                    placeholderText: qsTr("Votre nom (ex. Sami)")
                }
                LoginField {
                    id: setupPinField
                    placeholderText: qsTr("PIN (4 à 6 chiffres)")
                    echoMode: TextInput.Password
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                }
                LoginField {
                    id: confirmPinField
                    placeholderText: qsTr("Confirmez le PIN")
                    echoMode: TextInput.Password
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    onAccepted: setupButton.clicked()
                }

                NButton {
                    id: setupButton
                    Layout.fillWidth: true
                    large: true
                    text: qsTr("Créer et commencer")
                    onClicked: {
                        page.errorText = "";
                        Auth.setupManager(nameField.text, setupPinField.text,
                                          confirmPinField.text);
                    }
                }
            }

            // ── Connexion : tuiles utilisateur + PIN ──────────
            ColumnLayout {
                visible: !Auth.needsSetup
                Layout.fillWidth: true
                spacing: NTheme.s3

                Flow {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: NTheme.s2

                    Repeater {
                        model: Auth.users

                        delegate: Rectangle {
                            id: userTile

                            required property var modelData

                            readonly property bool selected:
                                page.selectedUserId === modelData.id

                            width: 128
                            height: 76
                            radius: NTheme.radiusCard
                            color: selected ? NTheme.primaryContainer
                                            : NTheme.surfaceCard
                            border.width: selected ? 2 : 1
                            border.color: selected ? NTheme.primary
                                                   : NTheme.outline

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "👤"
                                    font.pixelSize: 20
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: userTile.modelData.name
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    font.weight: Font.DemiBold
                                    color: NTheme.textPrimary
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: userTile.modelData.role === "manager"
                                        ? qsTr("Gérant")
                                        : userTile.modelData.role === "seller"
                                          ? qsTr("Vendeur") : qsTr("Ouvrier")
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: NTheme.textSecondary
                                }
                            }

                            TapHandler {
                                onTapped: {
                                    page.selectedUserId = userTile.modelData.id;
                                    page.errorText = "";
                                    pinField.forceActiveFocus();
                                }
                            }
                        }
                    }
                }

                NTextField {
                    id: pinField

                    Layout.fillWidth: true
                    enabled: page.selectedUserId > 0
                    placeholderText: page.selectedUserId > 0
                        ? qsTr("PIN") : qsTr("Choisissez d'abord votre nom")
                    echoMode: TextInput.Password
                    horizontalAlignment: TextInput.AlignHCenter
                    font.pixelSize: NTheme.fontSizeTitle
                    font.letterSpacing: 6
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    onAccepted: loginButton.clicked()
                }

                NButton {
                    id: loginButton
                    Layout.fillWidth: true
                    large: true
                    text: qsTr("Se connecter")
                    enabled: page.selectedUserId > 0 && pinField.text.length >= 4
                    onClicked: {
                        if (Auth.login(page.selectedUserId, pinField.text))
                            pinField.text = "";
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: page.errorText.length > 0
                text: page.errorText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.danger
            }
        }
    }
}
