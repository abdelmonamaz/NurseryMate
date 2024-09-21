#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>

namespace nursera {

// Décodage QR depuis le flux caméra (appairage F09-09 bis). La connexion
// à QVideoSink::videoFrameChanged se fait en C++ natif (Q_PROPERTY
// videoSink assigné depuis QML) plutôt que via un Connections QML : passer
// un QVideoFrame en argument d'un handler QML JS échoue silencieusement
// ("Could not convert argument 0 from undefined to QVideoFrame") sur cette
// chaîne Qt/Android — le marshalling JS↔C++ ne survit pas au type. On
// décode au plus 4x/s (ZXing-cpp coûte cher, inutile à 30 fps) et on émet
// le texte dès qu'un QR valide est détecté (format attendu nursera://IP:PORT).
class QrScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink
                   NOTIFY videoSinkChanged)

public:
    using QObject::QObject;

    QVideoSink* videoSink() const { return m_sink; }
    void setVideoSink(QVideoSink* sink);

signals:
    void videoSinkChanged();
    void decoded(const QString& text);

private slots:
    void processFrame(const QVideoFrame& frame);

private:
    QVideoSink* m_sink = nullptr;
    QElapsedTimer m_throttle;
};

} // namespace nursera
