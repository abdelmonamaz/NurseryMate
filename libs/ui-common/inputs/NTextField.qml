import QtQuick
import QtQuick.Controls.Basic

// Champ de saisie du design system (doc 03 §3) : bordure primaire au
// focus, thème appliqué. `fieldColor` s'adapte au fond (carte / page).
TextField {
    id: control

    property color fieldColor: NTheme.surfaceCard

    color: control.enabled ? NTheme.textPrimary : NTheme.textSecondary
    placeholderTextColor: NTheme.textSecondary
    font.family: NTheme.fontFamily
    font.pixelSize: NTheme.fontSizeBody

    background: Rectangle {
        radius: NTheme.radiusButton
        color: control.enabled ? control.fieldColor : NTheme.surface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? NTheme.primary : NTheme.outline
    }
}
