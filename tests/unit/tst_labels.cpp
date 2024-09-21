#include "generators/label_generator.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestLabels : public QObject
{
    Q_OBJECT

private slots:
    void emptyRejected();
    void pdfGenerated();
    void manyLabelsPaginate();
};

void TestLabels::emptyRejected()
{
    QTemporaryDir dir;
    QVERIFY(!LabelGenerator::generatePdf({}, dir.path()).isOk());
}

void TestLabels::pdfGenerated()
{
    QTemporaryDir dir;
    QList<LabelGenerator::Label> labels;
    labels.append({QStringLiteral("6191234567890"),
                   QStringLiteral("Citronnier 4 saisons — pot21"),
                   QStringLiteral("25,000 DT"), QStringLiteral("P0001-POT21")});
    labels.append({QStringLiteral("P0002-GODET"),
                   QStringLiteral("Romarin — godet"),
                   QStringLiteral("3,500 DT"), QStringLiteral("P0002-GODET")});

    const auto pdf = LabelGenerator::generatePdf(labels, dir.path());
    QVERIFY2(pdf.isOk(), qPrintable(pdf.isOk() ? QString() : pdf.error().message));
    const QFileInfo file(pdf.value());
    QVERIFY(file.exists());
    QVERIFY2(file.size() > 1000, qPrintable(QString::number(file.size())));
    QVERIFY(file.fileName().startsWith(QStringLiteral("etiquettes-")));
}

void TestLabels::manyLabelsPaginate()
{
    // 30 étiquettes -> 2 pages (24/page) sans planter
    QTemporaryDir dir;
    QList<LabelGenerator::Label> labels;
    for (int i = 0; i < 30; ++i)
        labels.append({QStringLiteral("SKU-%1").arg(i),
                       QStringLiteral("Produit %1").arg(i),
                       QStringLiteral("10,000 DT"),
                       QStringLiteral("SKU-%1").arg(i)});
    QVERIFY(LabelGenerator::generatePdf(labels, dir.path()).isOk());
}

// QTEST_MAIN : rendu QPainter -> PDF nécessite QGuiApplication.
QTEST_MAIN(TestLabels)
#include "tst_labels.moc"
