import QtQuick
import QtQuick.Layouts

// État vide : illustration + message + action optionnelle (doc 03 §6).
ColumnLayout {
    id: control

    property string emoji
    property string message
    property string actionText: ""
    // Un message long passe à la ligne au lieu de déborder du conteneur.
    property real maxTextWidth: 380

    signal actionClicked

    spacing: NTheme.s3

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: control.emoji
        font.pixelSize: 48
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: control.maxTextWidth
        text: control.message
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.family: NTheme.fontFamily
        font.pixelSize: NTheme.fontSizeBody
        color: NTheme.textSecondary
    }
    NButton {
        Layout.alignment: Qt.AlignHCenter
        visible: control.actionText.length > 0
        text: control.actionText
        onClicked: control.actionClicked()
    }
}
