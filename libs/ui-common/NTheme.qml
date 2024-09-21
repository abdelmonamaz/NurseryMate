pragma Singleton
import QtQuick

// Jetons du design system (doc 03 §2-3).
// Palette extraite de logo.jpeg — Pépinière Idéale.
QtObject {
    // ── Couleurs ──────────────────────────────────────────────
    readonly property color primary: "#3E7D14"          // vert foncé du dégradé (AA sur blanc)
    readonly property color primaryDark: "#2C5E0E"      // hover / pressed
    readonly property color primaryContainer: "#E9F3DC" // fonds de sélection, chips
    readonly property color accent: "#C1CF2C"           // vert anis du logo — décor uniquement
    readonly property color leaf: "#85C13A"             // vert feuille — badges positifs
    readonly property color surface: "#FAFBF7"
    readonly property color surfaceCard: "#FFFFFF"
    readonly property color outline: "#D9DDD2"
    readonly property color textPrimary: "#45454A"      // anthracite du logo
    readonly property color textSecondary: "#71767C"
    readonly property color success: "#3E7D14"
    readonly property color warning: "#B7791F"
    readonly property color danger: "#B3261E"
    readonly property color info: "#2B6CB0"

    // ── Espacements (échelle 4/8/12/16/24) ────────────────────
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24

    // ── Rayons & tailles ──────────────────────────────────────
    readonly property int radiusCard: 12
    readonly property int radiusButton: 8
    readonly property int buttonHeight: 40
    readonly property int buttonHeightLarge: 56   // cibles terrain ≥ 56 dp (doc 03 §1.2)

    // ── Typographie ───────────────────────────────────────────
    readonly property string fontFamily: "Inter"
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeSubtitle: 16
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeKpi: 28
    readonly property int fontSizeTotal: 40

    // ── Animations ────────────────────────────────────────────
    readonly property int animFast: 150
}
