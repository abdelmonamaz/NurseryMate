import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Dialogue standard : fond thémé, titre, zone d'erreur, pied
// Annuler / Valider. Le contenu reste libre (contentItem).
// La page ne ferme le dialogue que sur succès (onAcceptClicked).
Dialog {
    id: control

    property string errorText: ""
    property string acceptText: qsTr("Valider")
    property string cancelText: qsTr("Annuler")
    property bool acceptEnabled: true
    property bool showCancel: true   // false : dialogue de consultation

    signal acceptClicked
    signal cancelClicked

    anchors.centerIn: Overlay.overlay
    modal: true

    background: Rectangle {
        radius: NTheme.radiusCard
        color: NTheme.surfaceCard
        border.width: 1
        border.color: NTheme.outline
    }

    header: Text {
        text: control.title
        padding: NTheme.s4
        bottomPadding: NTheme.s2
        // Un titre long passe à la ligne au lieu de déborder du dialogue.
        wrapMode: Text.WordWrap
        font.family: NTheme.fontFamily
        font.pixelSize: NTheme.fontSizeTitle
        font.weight: Font.Bold
        color: NTheme.textPrimary
    }

    footer: ColumnLayout {
        spacing: NTheme.s2

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: NTheme.s4
            Layout.rightMargin: NTheme.s4
            visible: control.errorText.length > 0
            text: control.errorText
            wrapMode: Text.WordWrap
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeSmall
            color: NTheme.danger
        }

        RowLayout {
            spacing: NTheme.s2

            Item { Layout.fillWidth: true }

            NButton {
                // cancelText vide = dialogue de consultation, pas de bouton
                // (sinon le ghost à contour dessine un carré vide).
                visible: control.showCancel && control.cancelText.length > 0
                text: control.cancelText
                variant: "ghost"
                onClicked: {
                    control.cancelClicked();
                    control.close();
                }
            }
            NButton {
                text: control.acceptText
                enabled: control.acceptEnabled
                onClicked: control.acceptClicked()
            }
            Item { width: NTheme.s2 }
        }

        Item { height: 1 }
    }
}
