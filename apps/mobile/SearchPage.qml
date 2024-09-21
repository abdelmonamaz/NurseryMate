import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Recherche terrain (F09-02) : où est ce plant, combien il en reste —
// entièrement hors ligne sur la réplique locale.
Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: NTheme.s3
        spacing: NTheme.s2

        RowLayout {
            spacing: NTheme.s2
            NIconButton {
                text: "←"
                implicitHeight: 44
                onClicked: page.StackView.view.pop()
            }
            NTextField {
                id: searchField
                Layout.fillWidth: true
                implicitHeight: NTheme.buttonHeightLarge
                fieldColor: NTheme.surfaceCard
                placeholderText: qsTr("Nom, SKU ou douchette…")
                onTextEdited: resultList.model =
                    text.length >= 2 ? Sync.searchVariants(text) : []
            }
        }

        ListView {
            id: resultList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: NTheme.s2
            boundsBehavior: Flickable.StopAtBounds

            delegate: NCard {
                id: card

                required property var modelData

                width: ListView.view.width
                height: cardColumn.implicitHeight + NTheme.s3 * 2

                ColumnLayout {
                    id: cardColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: NTheme.s3
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        // Photo répliquée à la sync (reconnaître le plant)
                        Image {
                            visible: card.modelData.photoUrl.length > 0
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            fillMode: Image.PreserveAspectCrop
                            source: card.modelData.photoUrl
                            asynchronous: true
                            smooth: true
                        }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: card.modelData.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                        Text {
                            text: card.modelData.totalQty
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeTitle
                            font.weight: Font.Bold
                            color: card.modelData.totalQty > 0
                                ? NTheme.primary : NTheme.danger
                        }
                    }
                    Text {
                        text: card.modelData.sku
                        font.family: NTheme.fontFamily
                        font.pixelSize: NTheme.fontSizeSmall
                        color: NTheme.textSecondary
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: NTheme.s1
                        Repeater {
                            model: card.modelData.levels
                            delegate: NBadge {
                                required property var modelData
                                text: modelData.location + " : " + modelData.qty
                                badgeColor: modelData.qty > 0
                                    ? NTheme.leaf : NTheme.danger
                            }
                        }
                    }
                }
            }

            NEmptyState {
                anchors.centerIn: parent
                visible: resultList.count === 0
                emoji: "🔍"
                message: searchField.text.length >= 2
                    ? qsTr("Aucun résultat — synchronisez si le catalogue a changé.")
                    : qsTr("Tapez 2 lettres pour chercher.")
            }
        }
    }
}
