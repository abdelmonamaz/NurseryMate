import QtQuick
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Navigation latérale, filtrée selon le rôle (matrice des permissions,
// doc 01) : l'Ouvrier ne voit pas la Caisse.
Rectangle {
    id: sidebar

    property int currentSection: 0

    Layout.preferredWidth: 210
    Layout.fillHeight: true
    color: NTheme.surfaceCard

    component NavItem: Rectangle {
        id: navItem

        property string emoji
        property string label
        property int section

        Layout.fillWidth: true
        Layout.preferredHeight: 44
        radius: NTheme.radiusButton
        color: sidebar.currentSection === navItem.section
            ? NTheme.primaryContainer : "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: NTheme.s3
            spacing: NTheme.s2

            Text { text: navItem.emoji; font.pixelSize: 18 }
            Text {
                text: navItem.label
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                font.weight: sidebar.currentSection === navItem.section
                    ? Font.DemiBold : Font.Normal
                color: sidebar.currentSection === navItem.section
                    ? NTheme.primaryDark : NTheme.textPrimary
            }
            Item { Layout.fillWidth: true }
        }

        TapHandler { onTapped: sidebar.currentSection = navItem.section }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s2
        spacing: NTheme.s1

        NavItem { emoji: "⌂"; label: qsTr("Accueil"); section: 0 }
        NavItem {
            emoji: "🛒"; label: qsTr("Caisse"); section: 1
            visible: Auth.currentRole !== "worker"
        }
        NavItem { emoji: "🌿"; label: qsTr("Catalogue"); section: 2 }
        NavItem { emoji: "📦"; label: qsTr("Stock"); section: 3 }
        NavItem { emoji: "📋"; label: qsTr("Inventaire"); section: 4 }
        NavItem { emoji: "🌱"; label: qsTr("Production"); section: 8 }
        NavItem {
            emoji: "👥"; label: qsTr("Clients"); section: 5
            visible: Auth.currentRole !== "worker"
        }
        NavItem {
            emoji: "🧾"; label: qsTr("Devis"); section: 9
            visible: Auth.currentRole !== "worker"
        }
        NavItem {
            emoji: "🚚"; label: qsTr("Achats"); section: 6
            visible: Auth.currentRole === "manager"
        }
        NavItem {
            emoji: "📊"; label: qsTr("Rapports"); section: 10
            visible: Auth.currentRole === "manager"
        }
        Item { Layout.fillHeight: true }
        NavItem {
            emoji: "⚙️"; label: qsTr("Réglages"); section: 7
            visible: Auth.currentRole === "manager"
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: NTheme.outline
    }
}
