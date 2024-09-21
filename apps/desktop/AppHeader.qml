import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Barre supérieure : identité de l'entreprise + session (doc 03 §4.1).
Rectangle {
    height: 64
    color: NTheme.surfaceCard

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: NTheme.s4
        anchors.rightMargin: NTheme.s4
        spacing: NTheme.s3

        Image {
            source: "qrc:/resources/logo.jpeg"
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: "Pépinière Idéale"
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeTitle
                font.weight: Font.Bold
                color: NTheme.textPrimary
            }
            Text {
                text: qsTr("Vente, Aménagement & Entretien")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }

        Item { Layout.fillWidth: true }

        NBadge {
            visible: Auth.authenticated
            text: "👤 " + Auth.currentUserName + " · " + (
                Auth.currentRole === "manager" ? qsTr("Gérant")
                : Auth.currentRole === "seller" ? qsTr("Vendeur")
                : qsTr("Ouvrier"))
            badgeColor: NTheme.primary
        }

        NButton {
            visible: Auth.authenticated
            text: qsTr("Se déconnecter")
            variant: "ghost"
            onClicked: Auth.logout()
        }

        NBadge {
            text: qsTr("v%1").arg(Qt.application.version)
            badgeColor: NTheme.info
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: NTheme.outline
    }
}
