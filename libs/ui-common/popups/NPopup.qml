import QtQuick
import QtQuick.Controls.Basic

// Popup léger thémé (menus contextuels, aides, aperçus) — pour un
// dialogue avec titre/pied Annuler-Valider, utiliser NDialog.
Popup {
    padding: NTheme.s3

    background: Rectangle {
        radius: NTheme.radiusCard
        color: NTheme.surfaceCard
        border.width: 1
        border.color: NTheme.outline
    }
}
