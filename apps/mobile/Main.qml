import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Compagnon terrain (M09) : offline-first — saisir ne requiert JAMAIS le
// réseau, la file part à la sync. 3 taps max par action (doc 03 §4.3).
ApplicationWindow {
    id: root

    width: 400
    height: 800
    visible: true
    title: qsTr("Pépinière Idéale — Terrain")
    color: NTheme.surface

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Connections {
        target: Sync
        function onSyncFinished(ok, message) {
            banner.show(ok ? qsTr("✅ Synchronisé") : "⚠ " + message,
                        ok ? NTheme.primaryContainer : Qt.alpha(NTheme.warning, 0.15));
        }
        function onMoveQueued() {
            banner.show(qsTr("✅ Saisie enregistrée — partira à la sync."),
                        NTheme.primaryContainer);
            stack.pop(null);
        }
        function onErrorOccurred(message) {
            banner.show("⚠ " + message, Qt.alpha(NTheme.warning, 0.15));
        }
    }

    header: Rectangle {
        // Le contenu se colle en haut (56px) sans marge de sécurité —
        // sous notch/status bar, il se fait recouvrir par l'horloge/la
        // batterie du système. On étend le fond et on ancre la ligne
        // de contrôles au bas de cette zone.
        height: 56 + SafeArea.margins.top
        color: NTheme.surfaceCard

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: NTheme.s3
            anchors.rightMargin: NTheme.s3
            height: 56
            spacing: NTheme.s2

            Image {
                source: "qrc:/resources/logo.jpeg"
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            ColumnLayout {
                spacing: 0
                Text {
                    text: "Pépinière Idéale"
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                    color: NTheme.textPrimary
                }
                Text {
                    visible: Sync.connected
                    text: Sync.userName + " · " + Sync.serverDisplay
                    font.family: NTheme.fontFamily
                    font.pixelSize: NTheme.fontSizeSmall
                    color: NTheme.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            // Indicateur de sync (F09-01) : en attente / à jour / hors ligne
            NBadge {
                text: !Sync.connected ? qsTr("Hors ligne")
                    : Sync.pendingCount > 0
                      ? qsTr("⏳ %1 à envoyer").arg(Sync.pendingCount)
                      : qsTr("À jour")
                badgeColor: !Sync.connected ? NTheme.warning
                    : Sync.pendingCount > 0 ? NTheme.info : NTheme.leaf
            }
            NIconButton {
                visible: Sync.connected
                enabled: !Sync.busy
                text: "🔄"
                implicitHeight: 40
                onClicked: Sync.syncNow()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: NTheme.outline
        }
    }

    // Bannière de retour (succès / avertissement)
    footer: Rectangle {
        id: banner

        function show(message, background) {
            bannerText.text = message;
            color = background;
            visible = true;
            bannerTimer.restart();
        }

        // Idem en bas : la barre de navigation gestuelle du système ne
        // doit pas recouvrir le texte de la bannière.
        height: visible ? 48 + SafeArea.margins.bottom : 0
        visible: false
        Timer { id: bannerTimer; interval: 3500; onTriggered: banner.visible = false }
        Text {
            id: bannerText
            anchors.top: parent.top
            anchors.topMargin: NTheme.s2
            anchors.left: parent.left
            anchors.leftMargin: NTheme.s3
            anchors.right: parent.right
            anchors.rightMargin: NTheme.s3
            elide: Text.ElideRight
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
            color: NTheme.textPrimary
        }
    }

    StackView {
        id: stack

        anchors.fill: parent
        initialItem: Sync.connected ? homePage : loginPage

        Connections {
            target: Sync
            function onLoginFinished(ok, message) {
                if (ok)
                    stack.replace(null, homePage);
            }
        }
    }

    Component { id: loginPage; LoginPage {} }

    Component {
        id: homePage

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: NTheme.s4
                spacing: NTheme.s3

                component HomeTile: NCard {
                    id: tile

                    property string emoji
                    property string label

                    signal clicked

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: NTheme.s2

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: tile.emoji
                            font.pixelSize: 40
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: tile.label
                            font.family: NTheme.fontFamily
                            font.pixelSize: NTheme.fontSizeSubtitle
                            font.weight: Font.DemiBold
                            color: NTheme.textPrimary
                        }
                    }

                    TapHandler { onTapped: tile.clicked() }
                }

                HomeTile {
                    emoji: "🔍"; label: qsTr("Chercher un plant")
                    onClicked: stack.push("SearchPage.qml")
                }
                HomeTile {
                    emoji: "🔄"; label: qsTr("Déplacer")
                    onClicked: stack.push("MovePage.qml", { mode: "transfer" })
                }
                HomeTile {
                    emoji: "⚠️"; label: qsTr("Déclarer une perte")
                    onClicked: stack.push("MovePage.qml", { mode: "loss" })
                }
                HomeTile {
                    emoji: "📋"; label: qsTr("Inventaire")
                    onClicked: stack.push("InventoryPage.qml")
                }
            }
        }
    }
}
