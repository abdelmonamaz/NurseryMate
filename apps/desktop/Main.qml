import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

ApplicationWindow {
    id: root

    width: 1200
    height: 760
    visible: true
    title: qsTr("Pépinière Idéale — Gestion")
    color: NTheme.surface

    // RTL automatique quand la langue AR sera active (doc 03 §5)
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    header: AppHeader {}

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppSidebar {
            id: sidebar
            currentSection: 0
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentSection

            HomePage {}      // 0
            SalesPage {}     // 1 — Caisse (M04)
            CatalogPage {}   // 2
            StockPage {}     // 3
            InventoryPage {} // 4
            CustomersPage {} // 5 — Clients (M06)
            SuppliersPage {} // 6 — Achats (M07)
            AdminPage {}     // 7 — Réglages (M11)
            BatchesPage {}   // 8 — Production (M08)
            QuotesPage {     // 9 — Devis (M05)
                onNavigateToSales: sidebar.currentSection = 1
            }
            ReportsPage {}   // 10 — Rapports (M10)
        }
    }

    // Garde d'authentification (M01)
    LoginPage {
        anchors.fill: parent
        visible: !Auth.authenticated
        z: 100
    }
}
