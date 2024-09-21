#include "mobile_store.h"
#include "qr_scanner.h"
#include "sync_client.h"

#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Pepiniere Ideale"));
    QGuiApplication::setApplicationName(QStringLiteral("Nursera Terrain"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STR));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Bold.ttf"));
    QGuiApplication::setFont(QFont(QStringLiteral("Inter")));

    // Base locale offline-first (doc 02 §5) : réplique du référentiel +
    // file d'attente des saisies terrain.
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    nursera::MobileStore store(dataDir + QStringLiteral("/mobile-store.db"));
    if (auto opened = store.open(); !opened) {
        qCritical("Base locale inaccessible [%s] %s",
                  qPrintable(opened.error().code),
                  qPrintable(opened.error().message));
        return 1;
    }

    nursera::SyncClient sync(store);
    nursera::QrScanner qrScanner;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Sync"), &sync);
    engine.rootContext()->setContextProperty(QStringLiteral("QrScanner"),
                                             &qrScanner);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Nursera.Mobile", "Main");

    return app.exec();
}
