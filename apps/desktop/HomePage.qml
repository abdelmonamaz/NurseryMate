import QtQuick
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Tableau de bord (M10, F10-01) — réservé au Gérant (matrice des
// permissions, doc 01) ; les autres rôles voient un accueil simple.
Item {
    id: page

    readonly property bool isManager: Auth.currentRole === "manager"

    Component.onCompleted: {
        if (isManager) {
            Dashboard.refresh();
            Reminders.refresh();
        }
    }

    Connections {
        target: Auth
        function onSessionChanged() {
            if (page.isManager) {
                Dashboard.refresh();
                Reminders.refresh();
            }
        }
    }
    Connections {
        target: Sales
        function onSaleCompleted() {
            if (page.isManager)
                Dashboard.refresh();
        }
        function onSaleCancelled() {
            if (page.isManager)
                Dashboard.refresh();
        }
    }
    // Tout flux qui touche le stock (inventaire validé, mouvement manuel,
    // réception, lot vendable…) rafraîchit Stock -> le tableau de bord
    // (alertes stock bas) suit, sans redémarrage.
    Connections {
        target: Stock
        function onRefreshed() {
            if (page.isManager)
                Dashboard.refresh();
        }
    }

    // ── Accueil simple (Vendeur / Ouvrier) ────────────────────
    ColumnLayout {
        anchors.centerIn: parent
        visible: !page.isManager
        spacing: NTheme.s3

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Bienvenue %1 🌱").arg(Auth.currentUserName)
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeKpi
            font.weight: Font.Bold
            color: NTheme.textPrimary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Choisissez une section dans le menu.")
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            color: NTheme.textSecondary
        }
    }

    // ── Tableau de bord Gérant ────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s4
        visible: page.isManager
        spacing: NTheme.s3

        // Cartes KPI
        RowLayout {
            Layout.fillWidth: true
            spacing: NTheme.s3

            component KpiCard: NCard {
                property string title
                property string value
                property string hint: ""
                property color valueColor: NTheme.primaryDark

                Layout.fillWidth: true
                Layout.preferredHeight: 96

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: 2

                    NFieldLabel { text: title }
                    Text {
                        text: value
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeKpi
                        font.weight: Font.Bold
                        color: valueColor
                    }
                    Text {
                        visible: hint.length > 0
                        text: hint
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            KpiCard {
                title: qsTr("CA AUJOURD'HUI")
                value: Dashboard.kpis.todayTotal || "0,000 DT"
                hint: qsTr("%n vente(s)", "", Dashboard.kpis.todayCount || 0)
            }
            KpiCard {
                title: qsTr("PANIER MOYEN")
                value: Dashboard.kpis.averageBasket || "—"
                valueColor: NTheme.textPrimary
            }
            KpiCard {
                title: qsTr("CA 7 JOURS")
                value: Dashboard.kpis.weekTotal || "0,000 DT"
                valueColor: NTheme.textPrimary
            }
            KpiCard {
                title: qsTr("CA DU MOIS")
                value: Dashboard.kpis.monthTotal || "0,000 DT"
            }
        }

        // Graphiques : courbe CA (14 j) + diagramme mensuel (6 mois).
        // fillHeight explicite à false : un layout imbriqué s'étire par
        // défaut et écraserait les panneaux du bas (piège documenté).
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 190
            Layout.maximumHeight: 220
            spacing: NTheme.s3

            NCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    Text {
                        text: qsTr("📈 Chiffre d'affaires — 14 derniers jours")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }
                    NLineChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: Dashboard.salesDaily
                    }
                }
            }

            NCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    Text {
                        text: qsTr("📊 CA mensuel")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }
                    NBarChart {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        points: Dashboard.salesMonthly
                    }
                }
            }
        }

        // Deux panneaux : top produits & alertes stock
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: NTheme.s3

            // ── Top produits (30 j) ───────────────────────────
            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    Text {
                        text: qsTr("🏆 Top produits — 30 jours")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }

                    ListView {
                        id: topList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Dashboard.topProducts
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: topRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 44
                            hoverable: false
                            showSeparator: index < topList.count - 1

                            Text {
                                text: (topRow.index + 1) + "."
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.leaf
                            }
                            Text {
                                Layout.fillWidth: true
                                text: topRow.modelData.label
                                elide: Text.ElideRight
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textPrimary
                            }
                            Text {
                                text: qsTr("× %1").arg(topRow.modelData.qtySold)
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSmall
                                color: NTheme.textSecondary
                            }
                            Text {
                                text: topRow.modelData.revenue
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.Bold
                                color: NTheme.primary
                            }
                        }

                        NEmptyState {
                            anchors.centerIn: parent
                            visible: topList.count === 0
                            emoji: "🏆"
                            message: qsTr("Les meilleures ventes apparaîtront ici.")
                        }
                    }
                }
            }

            // ── Alertes stock (F03-05, RG-03.b) ───────────────
            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    Text {
                        text: qsTr("⚠ Alertes stock")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSubtitle
                        font.weight: Font.Bold
                        color: NTheme.textPrimary
                    }

                    ListView {
                        id: lowList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Dashboard.lowStock
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: lowRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 44
                            hoverable: false
                            showSeparator: index < lowList.count - 1

                            Text {
                                Layout.fillWidth: true
                                text: lowRow.modelData.label
                                elide: Text.ElideRight
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textPrimary
                            }
                            NBadge {
                                text: lowRow.modelData.negative
                                    ? qsTr("Négatif") : qsTr("Stock bas")
                                badgeColor: lowRow.modelData.negative
                                    ? NTheme.danger : NTheme.warning
                            }
                            Text {
                                text: lowRow.modelData.qty
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeSubtitle
                                font.weight: Font.Bold
                                color: lowRow.modelData.negative
                                    ? NTheme.danger : NTheme.warning
                            }
                        }

                        NEmptyState {
                            anchors.centerIn: parent
                            visible: lowList.count === 0
                            emoji: "✅"
                            message: qsTr("Aucune alerte — le stock est sain.")
                        }
                    }
                }
            }

            // ── Rappels d'entretien (F08-04) ──────────────────
            NCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: Reminders.overdueCount > 0
                                ? qsTr("🔔 Rappels — %n en retard", "",
                                       Reminders.overdueCount)
                                : qsTr("🔔 Rappels")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: Reminders.overdueCount > 0
                                ? NTheme.danger : NTheme.textPrimary
                        }
                        NIconButton {
                            text: "＋"
                            implicitHeight: 30
                            onClicked: reminderDialog.openNew()
                        }
                    }

                    ListView {
                        id: reminderList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Reminders.reminders
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: NListRow {
                            id: reminderRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: 44
                            hoverable: false
                            showSeparator: index < reminderList.count - 1

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: reminderRow.modelData.label
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeBody
                                    color: NTheme.textPrimary
                                }
                                Text {
                                    text: reminderRow.modelData.dueDate
                                        + (reminderRow.modelData.batchNumber.length > 0
                                           ? "  ·  " + reminderRow.modelData.batchNumber
                                           : "")
                                    font.family: NTheme.fontFamily
                                    font.pixelSize: NTheme.fontSizeSmall
                                    color: reminderRow.modelData.overdue
                                        ? NTheme.danger : NTheme.textSecondary
                                }
                            }
                            NBadge {
                                visible: reminderRow.modelData.overdue
                                text: qsTr("En retard")
                                badgeColor: NTheme.danger
                            }
                            NIconButton {
                                text: "✓"
                                implicitHeight: 30
                                onClicked: Reminders.markDone(
                                    reminderRow.modelData.id)
                            }
                        }

                        NEmptyState {
                            anchors.centerIn: parent
                            visible: reminderList.count === 0
                            emoji: "🔔"
                            message: qsTr("Aucun rappel — planifiez-en depuis une intervention ou avec ＋.")
                        }
                    }
                }
            }
        }
    }

    // ── Nouveau rappel libre (F08-04) ─────────────────────────
    NDialog {
        id: reminderDialog

        function openNew() {
            reminderLabel.text = "";
            reminderDue.text = "";
            errorText = "";
            open();
        }

        width: 420
        title: qsTr("Nouveau rappel")
        acceptText: qsTr("Planifier")
        acceptEnabled: reminderLabel.text.trim().length > 0
                       && reminderDue.dateValid && reminderDue.text.length > 0
        onOpened: reminderLabel.forceActiveFocus()
        onAcceptClicked: {
            if (Reminders.addReminder(reminderLabel.text, reminderDue.text, 0))
                close();
        }

        contentItem: ColumnLayout {
            spacing: NTheme.s2

            NFieldLabel { text: qsTr("Quoi faire *") }
            NTextField {
                id: reminderLabel
                Layout.fillWidth: true
                fieldColor: NTheme.surface
                placeholderText: qsTr("ex. fertiliser Serre 1")
            }
            NFieldLabel { text: qsTr("Pour le *") }
            NDateField {
                id: reminderDue
                Layout.preferredWidth: 150
                fieldColor: NTheme.surface
                required: true
            }
        }
    }
}
