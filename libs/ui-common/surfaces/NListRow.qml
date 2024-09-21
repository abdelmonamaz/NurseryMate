import QtQuick
import QtQuick.Layouts

// Ligne de liste standard (doc 03 §3) : hover, séparateur bas,
// contenu en RowLayout (RTL-safe). Les enfants déclarés dans le
// composant atterrissent dans le RowLayout interne.
Rectangle {
    id: control

    property bool hoverable: true
    property bool showSeparator: true
    property color baseColor: "transparent"
    default property alias content: inner.data

    signal clicked

    height: 64
    color: control.hoverable && hover.hovered ? NTheme.surface : control.baseColor

    HoverHandler { id: hover }
    TapHandler { onTapped: control.clicked() }

    RowLayout {
        id: inner

        anchors.fill: parent
        anchors.leftMargin: NTheme.s4
        anchors.rightMargin: NTheme.s4
        spacing: NTheme.s3
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: NTheme.outline
        visible: control.showSeparator
    }
}
