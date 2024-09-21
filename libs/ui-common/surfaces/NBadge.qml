import QtQuick

// Badge de statut : stock bas, impayé, sync en attente… (doc 03 §3).
Rectangle {
    id: control

    property string text
    property color badgeColor: NTheme.info

    implicitHeight: 22
    implicitWidth: label.implicitWidth + NTheme.s4
    radius: height / 2
    color: Qt.alpha(control.badgeColor, 0.15)

    Text {
        id: label
        anchors.centerIn: parent
        text: control.text
        color: control.badgeColor
        font.family: NTheme.fontFamily
        font.pixelSize: NTheme.fontSizeSmall
        font.weight: Font.DemiBold
    }
}
