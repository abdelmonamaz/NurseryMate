import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Connexion au poste principal (IP affichée dans Réglages du desktop).
// Le PIN n'est jamais stocké sur le téléphone.
Item {
    id: page

    property bool discovering: false

    Connections {
        target: Sync
        function onLoginFinished(ok, message) {
            if (!ok)
                errorText.text = message;
        }
        function onDiscoverFinished(found, host, port) {
            page.discovering = false;
            if (found) {
                hostField.text = host;
                portField.text = port;
                errorText.text = "";
            } else {
                errorText.text = qsTr("Poste principal introuvable sur ce réseau — vérifiez le Wi-Fi et que la case « 📡 Poste principal » est cochée sur le PC.");
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - NTheme.s4 * 2, 360)
        spacing: NTheme.s3

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/resources/logo.jpeg"
            Layout.preferredWidth: 96
            Layout.preferredHeight: 96
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Connexion au poste principal")
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeTitle
            font.weight: Font.Bold
            color: NTheme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("L'adresse IP est affichée dans Réglages, sur le poste principal (case « 📡 Poste principal »).")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeSmall
            color: NTheme.textSecondary
        }

        // Découverte automatique (F09-09) : broadcast UDP sur le LAN
        NButton {
            Layout.fillWidth: true
            variant: "secondary"
            enabled: !page.discovering
            text: page.discovering ? qsTr("Recherche…")
                                   : qsTr("🔍 Chercher le poste sur le réseau")
            onClicked: {
                page.discovering = true;
                errorText.text = "";
                Sync.discover(portField.text);
            }
        }

        // Appairage par QR (F09-09 bis) : utile si le mobile et le poste
        // ne sont pas sur le même réseau (partage de connexion, etc.).
        NButton {
            Layout.fillWidth: true
            variant: "secondary"
            text: qsTr("📷 Scanner le QR du poste principal")
            onClicked: {
                const scanPage = page.StackView.view.push("ScanPage.qml");
                scanPage.scanned.connect(function(host, port) {
                    hostField.text = host;
                    portField.text = port;
                    errorText.text = "";
                    page.StackView.view.pop();
                });
            }
        }

        RowLayout {
            spacing: NTheme.s2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Adresse IP *") }
                NTextField {
                    id: hostField
                    Layout.fillWidth: true
                    implicitHeight: NTheme.buttonHeightLarge
                    fieldColor: NTheme.surfaceCard
                    text: Sync.savedHost()
                    placeholderText: "192.168.1.10"
                    inputMethodHints: Qt.ImhPreferNumbers
                }
            }
            ColumnLayout {
                spacing: NTheme.s1
                NFieldLabel { text: qsTr("Port") }
                NTextField {
                    id: portField
                    Layout.preferredWidth: 90
                    implicitHeight: NTheme.buttonHeightLarge
                    fieldColor: NTheme.surfaceCard
                    text: Sync.savedPort()
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
            }
        }

        NFieldLabel { text: qsTr("Utilisateur *") }
        NTextField {
            id: userField
            Layout.fillWidth: true
            implicitHeight: NTheme.buttonHeightLarge
            fieldColor: NTheme.surfaceCard
            placeholderText: qsTr("ex. sami")
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
        }

        NFieldLabel { text: qsTr("PIN *") }
        NTextField {
            id: pinField
            Layout.fillWidth: true
            implicitHeight: NTheme.buttonHeightLarge
            fieldColor: NTheme.surfaceCard
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
        }

        Text {
            id: errorText
            visible: text.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeSmall
            color: NTheme.danger
        }

        NButton {
            Layout.fillWidth: true
            large: true
            enabled: !Sync.busy && hostField.text.trim().length > 0
                     && userField.text.trim().length > 0
                     && pinField.text.length >= 4
            text: Sync.busy ? qsTr("Connexion…") : qsTr("Se connecter")
            onClicked: {
                errorText.text = "";
                Sync.login(hostField.text, portField.text,
                           userField.text, pinField.text);
            }
        }
    }
}
