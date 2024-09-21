#include "qr_scanner.h"

#include <QImage>

#include <ReadBarcode.h>

namespace nursera {

void QrScanner::setVideoSink(QVideoSink* sink)
{
    if (m_sink == sink)
        return;
    if (m_sink)
        disconnect(m_sink, nullptr, this, nullptr);
    m_sink = sink;
    if (m_sink)
        connect(m_sink, &QVideoSink::videoFrameChanged, this,
                &QrScanner::processFrame);
    emit videoSinkChanged();
}

void QrScanner::processFrame(const QVideoFrame& frameIn)
{
    if (m_throttle.isValid() && m_throttle.elapsed() < 400)
        return;
    m_throttle.restart();

    QVideoFrame frame = frameIn;
    const QImage image = frame.toImage();
    if (image.isNull())
        return;
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);

    const ZXing::ImageView view(gray.constBits(), gray.width(), gray.height(),
                                ZXing::ImageFormat::Lum, int(gray.bytesPerLine()));
    const auto options =
        ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode);
    const ZXing::Barcode barcode = ZXing::ReadBarcode(view, options);
    if (barcode.isValid())
        emit decoded(QString::fromStdString(barcode.text()));
}

} // namespace nursera
