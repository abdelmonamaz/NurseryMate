import QtQuick
import QtQuick.Controls.Basic

// Bouton du design system (doc 03 §3).
// variant : primary | secondary | ghost | danger — large : cibles terrain 56 dp.
// Tous les variants ont une affordance visible au repos (fond ou contour) ;
// les changements d'état (hover/appui) sont instantanés — pas d'animation
// de couleur, sinon l'aller-retour appui/relâche donne un double clignotement.
Button {
    id: control

    property string variant: "primary"
    property bool large: false

    readonly property color _background: {
        if (variant === "primary")
            return control.down ? NTheme.primaryDark
                 : control.hovered ? Qt.darker(NTheme.primary, 1.08)
                 : NTheme.primary;
        if (variant === "danger")
            return control.down ? Qt.darker(NTheme.danger, 1.2)
                 : control.hovered ? Qt.darker(NTheme.danger, 1.08)
                 : NTheme.danger;
        if (variant === "secondary")
            return control.down ? NTheme.primaryContainer
                 : control.hovered ? NTheme.surface
                 : NTheme.surfaceCard;
        // ghost : discret mais identifiable — contour permanent, fond au survol
        return control.down ? Qt.darker(NTheme.primaryContainer, 1.05)
             : control.hovered ? NTheme.primaryContainer
             : "transparent";
    }
    readonly property color _foreground:
        variant === "primary" || variant === "danger" ? "#FFFFFF" : NTheme.primary

    implicitHeight: large ? NTheme.buttonHeightLarge : NTheme.buttonHeight
    leftPadding: NTheme.s4
    rightPadding: NTheme.s4

    contentItem: Text {
        text: control.text
        font.family: NTheme.fontFamily
        font.pixelSize: control.large ? NTheme.fontSizeSubtitle : NTheme.fontSizeBody
        font.weight: Font.Medium
        color: control._foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1.0 : 0.5
    }

    background: Rectangle {
        radius: NTheme.radiusButton
        color: control._background
        border.width: control.variant === "secondary"
                      || control.variant === "ghost" ? 1 : 0
        border.color: NTheme.outline
        opacity: control.enabled ? 1.0 : 0.5
    }
}
