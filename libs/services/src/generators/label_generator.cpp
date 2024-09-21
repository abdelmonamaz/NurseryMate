#include "generators/label_generator.h"

#include "qrcodegen.hpp"

#include <QDateTime>
#include <QDir>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace nursera {
namespace {

// Dessine le QR de `data` dans le carré (x, y, size) en points.
void drawQr(QPainter& painter, const QString& data, qreal x, qreal y, qreal size)
{
    using qrcodegen::QrCode;
    const QrCode qr = QrCode::encodeText(data.toUtf8().constData(),
                                         QrCode::Ecc::MEDIUM);
    const int n = qr.getSize();
    if (n <= 0)
        return;
    const qreal cell = size / n;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x1F, 0x2B, 0x22)); // encre sombre
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            if (qr.getModule(c, r))
                painter.drawRect(QRectF(x + c * cell, y + r * cell, cell, cell));
}

} // namespace

Result<QString> LabelGenerator::generatePdf(const QList<Label>& labels,
                                            const QString& outputDir)
{
    if (labels.isEmpty())
        return Result<QString>::fail(QStringLiteral("label.empty"),
                                     QStringLiteral("Aucune étiquette à imprimer."));
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("label.dir"), outputDir);

    const QString path = outputDir + QStringLiteral("/etiquettes-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QStringLiteral(".pdf");

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    writer.setResolution(300);

    QPainter painter;
    if (!painter.begin(&writer))
        return Result<QString>::fail(QStringLiteral("label.painter"),
                                     QStringLiteral("Impossible d'ouvrir le PDF."));

    const int cols = 3;
    const int rows = 8;
    const int perPage = cols * rows;
    const qreal pageW = writer.width();
    const qreal pageH = writer.height();
    const qreal cellW = pageW / cols;
    const qreal cellH = pageH / rows;
    const qreal qrSize = qMin(cellW, cellH) * 0.55;

    QFont nameFont(QStringLiteral("Inter"));
    nameFont.setPixelSize(int(cellH * 0.11));
    nameFont.setBold(true);
    QFont smallFont(QStringLiteral("Inter"));
    smallFont.setPixelSize(int(cellH * 0.09));

    for (int i = 0; i < labels.size(); ++i) {
        if (i > 0 && i % perPage == 0)
            writer.newPage();
        const int idx = i % perPage;
        const qreal cx = (idx % cols) * cellW;
        const qreal cy = (idx / cols) * cellH;

        // Cadre discret (repère de découpe)
        painter.setPen(QPen(QColor(0xD8, 0xDC, 0xD4), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(cx + 4, cy + 4, cellW - 8, cellH - 8));

        const Label& label = labels.at(i);

        // QR à gauche
        const qreal qrY = cy + (cellH - qrSize) / 2;
        drawQr(painter, label.qrData.isEmpty() ? label.sku : label.qrData,
               cx + 14, qrY, qrSize);

        // Texte à droite du QR
        const qreal tx = cx + 14 + qrSize + 12;
        const qreal tw = cellW - (14 + qrSize + 12) - 10;
        painter.setPen(QColor(0x1F, 0x2B, 0x22));
        painter.setFont(nameFont);
        painter.drawText(QRectF(tx, cy + cellH * 0.20, tw, cellH * 0.30),
                         Qt::TextWordWrap, label.nameFr);
        painter.setFont(smallFont);
        painter.setPen(QColor(0x5B, 0x6A, 0x5F));
        painter.drawText(QRectF(tx, cy + cellH * 0.52, tw, cellH * 0.15),
                         Qt::AlignLeft, label.sku);
        painter.setFont(nameFont);
        painter.setPen(QColor(0x3E, 0x7D, 0x14));
        painter.drawText(QRectF(tx, cy + cellH * 0.64, tw, cellH * 0.22),
                         Qt::AlignLeft, label.priceDisplay);
    }

    painter.end();
    return Result<QString>::ok(path);
}

} // namespace nursera
