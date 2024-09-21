import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtMultimedia
import Nursera.UI

// Appairage par QR (F09-09 bis) : scanne le code affiché dans Réglages
// sur le poste principal (nursera://IP:PORT) — alternative à la
// découverte UDP quand les deux appareils ne sont pas sur le même Wi-Fi
// (partage de connexion, réseaux invités isolés…).
Item {
    id: page

    signal scanned(string host, string port)

    Component.onDestruction: QrScanner.videoSink = null

    CameraPermission {
        id: cameraPermission
        Component.onCompleted: {
            if (status === Qt.PermissionStatus.Undetermined)
                request();
        }
    }

    CaptureSession {
        camera: Camera {
            active: cameraPermission.status === Qt.PermissionStatus.Granted
        }
        videoOutput: videoOutput
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        visible: cameraPermission.status === Qt.PermissionStatus.Granted

        // La connexion au flux se fait en C++ natif (QrScanner.videoSink,
        // voir qr_scanner.h) — un Connections QML sur videoFrameChanged
        // échoue à transmettre le QVideoFrame (undefined côté JS).
        Component.onCompleted: QrScanner.videoSink = videoOutput.videoSink
    }

    NEmptyState {
        anchors.centerIn: parent
        visible: cameraPermission.status === Qt.PermissionStatus.Denied
        emoji: "🚫"
        message: qsTr("Autorisation caméra refusée — activez-la dans les paramètres de l'application pour scanner un QR.")
    }

    // Cadre de visée
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.62
        height: width
        color: "transparent"
        border.color: NTheme.accent
        border.width: 3
        radius: NTheme.radiusCard
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: NTheme.s3
        spacing: NTheme.s2

        NIconButton {
            text: "←"
            implicitHeight: 44
            onClicked: page.StackView.view.pop()
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Visez le QR affiché dans Réglages, sur le poste principal (case « 📡 Poste principal »).")
            wrapMode: Text.WordWrap
            color: "white"
            style: Text.Outline
            styleColor: "black"
            font.family: NTheme.fontFamily
            font.pixelSize: NTheme.fontSizeBody
        }
    }

    Connections {
        target: QrScanner
        function onDecoded(text) {
            const payload = text.trim();
            if (!payload.startsWith("nursera://"))
                return;
            const rest = payload.substring("nursera://".length);
            const sep = rest.lastIndexOf(":");
            if (sep <= 0)
                return;
            page.scanned(rest.substring(0, sep), rest.substring(sep + 1));
        }
    }
}
