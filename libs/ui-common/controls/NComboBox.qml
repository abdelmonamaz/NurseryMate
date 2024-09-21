import QtQuick
import QtQuick.Controls.Basic

// Liste déroulante du design system : même langage que NTextField
// (fond carte, contour, focus primaire), chevron dessiné, liste
// déroulante en carte arrondie avec surlignage au survol.
ComboBox {
    id: control

    property color fieldColor: NTheme.surfaceCard

    font.family: NTheme.fontFamily
    font.pixelSize: NTheme.fontSizeBody
    implicitHeight: NTheme.buttonHeight
    leftPadding: NTheme.s3
    rightPadding: NTheme.s3 + 18

    background: Rectangle {
        radius: NTheme.radiusButton
        color: control.enabled ? control.fieldColor : NTheme.surface
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        border.color: control.activeFocus || control.popup.visible
            ? NTheme.primary
            : control.hovered ? NTheme.textSecondary : NTheme.outline
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.enabled ? NTheme.textPrimary : NTheme.textSecondary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // Chevron ˅ qui pivote à l'ouverture
    indicator: Canvas {
        x: control.width - width - NTheme.s3
        y: (control.height - height) / 2
        width: 12
        height: 7
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: NTheme.animFast } }

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.beginPath();
            ctx.moveTo(1, 1);
            ctx.lineTo(width / 2, height - 1);
            ctx.lineTo(width - 1, 1);
            ctx.strokeStyle = NTheme.textSecondary;
            ctx.lineWidth = 1.8;
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            ctx.stroke();
        }
    }

    delegate: ItemDelegate {
        id: item

        required property var model
        required property int index

        width: ListView.view.width
        height: 36
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: item.model[control.textRole] !== undefined
                  ? item.model[control.textRole] : item.model.display
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            font.weight: control.currentIndex === item.index
                ? Font.DemiBold : Font.Normal
            color: control.currentIndex === item.index
                ? NTheme.primary : NTheme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: NTheme.radiusButton - 2
            color: item.highlighted ? NTheme.primaryContainer : "transparent"
        }
    }

    popup: NPopup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + NTheme.s2 * 2, 320)
        padding: NTheme.s1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
