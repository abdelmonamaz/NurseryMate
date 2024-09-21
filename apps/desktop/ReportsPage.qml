import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI
import Nursera.App

// Écran Rapports (M10) : synthèse ventes, ventes par catégorie / paiement,
// valorisation du stock (CMP), pertes de production — export CSV (F10-02..06).
Item {
    id: page

    Component.onCompleted: Reports.refresh()

    // ── Tuile KPI ─────────────────────────────────────────────
    component Kpi: NCard {
        id: kpiCard
        property string label
        property string value
        property string sub
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: kpiCol.implicitHeight + NTheme.s3 * 2
        ColumnLayout {
            id: kpiCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: NTheme.s3
            spacing: 2
            Text {
                text: kpiCard.label
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
            Text {
                text: kpiCard.value
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeKpi
                font.weight: Font.Bold
                color: NTheme.primaryDark
            }
            Text {
                visible: kpiCard.sub.length > 0
                text: kpiCard.sub
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeSmall
                color: NTheme.textSecondary
            }
        }
    }

    // ── Section rapport (tableau + export CSV) ────────────────
    component ReportSection: NCard {
        id: section
        property string title
        property string reportKey
        property var rows: []
        property string columnLabel      // en-tête colonne "montant"
        property bool showValue: true
        property bool showSub: false
        property string footer            // ligne total optionnelle
        property var chartPoints: []      // barres optionnelles

        Layout.fillWidth: true
        implicitHeight: col.implicitHeight + NTheme.s3 * 2

        ColumnLayout {
            id: col
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: NTheme.s3
            spacing: NTheme.s2

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: section.title
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
                Item { Layout.fillWidth: true }
                NButton {
                    text: qsTr("⬇ CSV")
                    variant: "secondary"
                    enabled: section.rows.length > 0
                    onClicked: {
                        const url = Reports.exportCsv(section.reportKey);
                        if (url.length > 0) Qt.openUrlExternally(url);
                    }
                }
                NButton {
                    text: qsTr("📄 PDF")
                    variant: "secondary"
                    enabled: section.rows.length > 0
                    onClicked: {
                        const url = Reports.exportPdf(section.reportKey);
                        if (url.length > 0) Qt.openUrlExternally(url);
                    }
                }
            }

            NBarChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                visible: section.chartPoints.length > 0
                points: section.chartPoints
            }

            // En-têtes
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s2
                visible: section.rows.length > 0
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Libellé")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    Layout.preferredWidth: 70
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Qté")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
                Text {
                    visible: section.showValue
                    Layout.preferredWidth: 130
                    horizontalAlignment: Text.AlignRight
                    text: section.columnLabel
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            Repeater {
                model: section.rows
                delegate: RowLayout {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: NTheme.s2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: modelData.label
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeBody
                            color: NTheme.textPrimary
                        }
                        Text {
                            visible: section.showSub && modelData.sub.length > 0
                            text: modelData.sub
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }
                    Text {
                        Layout.preferredWidth: 70
                        horizontalAlignment: Text.AlignRight
                        text: modelData.qty
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        color: NTheme.textPrimary
                    }
                    Text {
                        visible: section.showValue
                        Layout.preferredWidth: 130
                        horizontalAlignment: Text.AlignRight
                        text: modelData.amount
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: NTheme.primary
                    }
                }
            }

            NEmptyState {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                visible: section.rows.length === 0
                emoji: "🗒️"
                message: qsTr("Aucune donnée sur la période choisie.")
            }

            // Ligne total
            RowLayout {
                Layout.fillWidth: true
                visible: section.footer.length > 0 && section.rows.length > 0
                Item { Layout.fillWidth: true }
                Text {
                    text: section.footer
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.primaryDark
                }
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + NTheme.s4 * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            width: parent.width - NTheme.s4 * 2
            x: NTheme.s4
            y: NTheme.s4
            spacing: NTheme.s3

            // Titre + sélecteur de période
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s3
                Text {
                    text: qsTr("Rapports")
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
                Item { Layout.fillWidth: true }
                NButton {
                    text: qsTr("Ce mois")
                    variant: "ghost"
                    onClicked: Reports.setThisMonth()
                }
                NButton {
                    text: qsTr("30 jours")
                    variant: "ghost"
                    onClicked: Reports.setLast30Days()
                }
                NFieldLabel { text: qsTr("Du") }
                NDateField {
                    id: fromField
                    Layout.preferredWidth: 130
                    fieldColor: NTheme.surfaceCard
                    required: true
                    text: Reports.fromDate
                    onEditingFinished: if (dateValid) Reports.fromDate = text
                }
                NFieldLabel { text: qsTr("au") }
                NDateField {
                    id: toField
                    Layout.preferredWidth: 130
                    fieldColor: NTheme.surfaceCard
                    required: true
                    text: Reports.toDate
                    onEditingFinished: if (dateValid) Reports.toDate = text
                }
            }

            // KPI de synthèse
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s3
                Kpi { label: qsTr("Ventes"); value: (Reports.salesSummary.count ?? 0).toString() }
                Kpi {
                    label: qsTr("Chiffre d'affaires net")
                    value: Reports.salesSummary.net ?? "—"
                    sub: (Reports.salesSummary.hasCredits ?? false)
                         ? qsTr("brut %1 · avoirs −%2")
                               .arg(Reports.salesSummary.total)
                               .arg(Reports.salesSummary.credits)
                         : ""
                }
                Kpi { label: qsTr("Panier moyen"); value: Reports.salesSummary.average ?? "—" }
            }

            // Ventes par catégorie + paiement (largeurs égales)
            RowLayout {
                Layout.fillWidth: true
                spacing: NTheme.s3

                ReportSection {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Ventes par catégorie")
                    reportKey: "sales_category"
                    rows: Reports.salesByCategory
                    columnLabel: qsTr("CA")
                    chartPoints: {
                        var out = [];
                        var list = Reports.salesByCategory;
                        for (var i = 0; i < list.length && i < 6; ++i)
                            out.push({
                                label: list[i].label.substring(0, 8),
                                value: list[i].amountMillimes,
                                display: list[i].amount,
                                fullLabel: list[i].label,
                            });
                        return out;
                    }
                }

                ReportSection {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Ventes par paiement")
                    reportKey: "sales_payment"
                    rows: Reports.salesByPayment
                    columnLabel: qsTr("Total")
                }
            }

            // Rapport de marge (F10-05) — colonnes multiples, section dédiée
            NCard {
                id: marginCard
                Layout.fillWidth: true
                implicitHeight: marginCol.implicitHeight + NTheme.s3 * 2

                ColumnLayout {
                    id: marginCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: NTheme.s3
                    spacing: NTheme.s2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: NTheme.s2

                        Text {
                            text: qsTr("Rapport de marge (CA − coût CMP)")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: NTheme.textPrimary
                        }
                        NButton {
                            text: qsTr("Par catégorie")
                            variant: Reports.marginBy === "category" ? "primary" : "ghost"
                            implicitHeight: 30
                            onClicked: Reports.marginBy = "category"
                        }
                        NButton {
                            text: qsTr("Par produit")
                            variant: Reports.marginBy === "product" ? "primary" : "ghost"
                            implicitHeight: 30
                            onClicked: Reports.marginBy = "product"
                        }
                        Item { Layout.fillWidth: true }
                        NButton {
                            text: qsTr("⬇ CSV")
                            variant: "secondary"
                            enabled: Reports.marginRows.length > 0
                            onClicked: {
                                const url = Reports.exportCsv("margin");
                                if (url.length > 0) Qt.openUrlExternally(url);
                            }
                        }
                        NButton {
                            text: qsTr("📄 PDF")
                            variant: "secondary"
                            enabled: Reports.marginRows.length > 0
                            onClicked: {
                                const url = Reports.exportPdf("margin");
                                if (url.length > 0) Qt.openUrlExternally(url);
                            }
                        }
                    }

                    // En-têtes
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: NTheme.s2
                        visible: Reports.marginRows.length > 0

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Libellé")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Text {
                            Layout.preferredWidth: 55
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("Qté")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Text {
                            Layout.preferredWidth: 115
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("CA")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Text {
                            Layout.preferredWidth: 115
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("Coût CMP")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Text {
                            Layout.preferredWidth: 115
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("Marge")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                        Text {
                            Layout.preferredWidth: 65
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("Taux")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSmall
                            color: NTheme.textSecondary
                        }
                    }

                    Repeater {
                        model: Reports.marginRows
                        delegate: RowLayout {
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
                                Layout.preferredWidth: 55
                                horizontalAlignment: Text.AlignRight
                                text: modelData.qty
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textPrimary
                            }
                            Text {
                                Layout.preferredWidth: 115
                                horizontalAlignment: Text.AlignRight
                                text: modelData.revenue
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textPrimary
                            }
                            Text {
                                Layout.preferredWidth: 115
                                horizontalAlignment: Text.AlignRight
                                text: modelData.cost
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: NTheme.textSecondary
                            }
                            Text {
                                Layout.preferredWidth: 115
                                horizontalAlignment: Text.AlignRight
                                text: modelData.margin
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: modelData.negative ? NTheme.danger : NTheme.success
                            }
                            Text {
                                Layout.preferredWidth: 65
                                horizontalAlignment: Text.AlignRight
                                text: modelData.rate
                                font.family: NTheme.fontFamily
                                font.pixelSize: NTheme.fontSizeBody
                                color: modelData.negative ? NTheme.danger : NTheme.textSecondary
                            }
                        }
                    }

                    NEmptyState {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        visible: Reports.marginRows.length === 0
                        emoji: "📈"
                        message: qsTr("Aucune vente sur la période — pas de marge à calculer.")
                    }

                    // Total
                    RowLayout {
                        Layout.fillWidth: true
                        visible: Reports.marginRows.length > 0
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("CA %1 · Marge %2 (%3)")
                                .arg(Reports.marginTotals.revenue || "")
                                .arg(Reports.marginTotals.margin || "")
                                .arg(Reports.marginTotals.rate || "")
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.Bold
                            color: NTheme.primaryDark
                        }
                    }

                    Text {
                        text: qsTr("Coût au CMP actuel des articles (approximation standard) ; les prestations sans produit comptent à 100 % de marge.")
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Valorisation du stock (CMP)
            ReportSection {
                title: qsTr("Valorisation du stock (CMP)")
                reportKey: "stock"
                rows: Reports.stockValuation
                columnLabel: qsTr("Valeur")
                showSub: true
                footer: qsTr("Total : %1").arg(Reports.stockValuationTotal)
            }

            // Pertes de production
            ReportSection {
                title: qsTr("Pertes de production")
                reportKey: "losses"
                rows: Reports.productionLosses
                columnLabel: ""
                showValue: false
            }
        }
    }

    Connections {
        target: Reports
        function onErrorOccurred(message) { errorBanner.show(message); }
    }

    // Bannière d'erreur non bloquante
    Rectangle {
        id: errorBanner
        function show(msg) { errorText.text = msg; visible = true; hideTimer.restart(); }
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter
            bottomMargin: NTheme.s4 }
        visible: false
        radius: NTheme.radiusButton
        color: NTheme.danger
        width: errorText.width + NTheme.s4 * 2
        height: errorText.height + NTheme.s3 * 2
        z: 50
        Text {
            id: errorText
            anchors.centerIn: parent
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            color: NTheme.surfaceCard
        }
        Timer { id: hideTimer; interval: 4000; onTriggered: errorBanner.visible = false }
    }
}
