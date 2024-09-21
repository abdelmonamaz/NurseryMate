import QtQuick

// Bouton carré pour icône/emoji seul (journaux, lignes de liste).
// Padding nul : un emoji (paire de substitution) ne s'élide jamais,
// contrairement à un NButton étroit (piège documenté : padding 16+16).
NButton {
    variant: "ghost"
    leftPadding: 0
    rightPadding: 0
    implicitWidth: implicitHeight
}
