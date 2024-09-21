#include "common/password_hasher.h"
#include "controllers/auth_controller.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_user_repository.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestAuth : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void hasherRoundTrip();
    void firstRunSetup();
    void loginAndLogout();
    void invalidPinsRejectedAtSetup();
    void lockoutAfterFiveFailures();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteUserRepository* m_users = nullptr;
    AuthController* m_auth = nullptr;
    int m_managerId = 0;
};

void TestAuth::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("auth.db")),
                               QStringLiteral("tst_auth"));
    QVERIFY(m_db->open().isOk());
    m_users = new SqliteUserRepository(m_db->connectionName());
    m_auth = new AuthController(*m_users);
}

void TestAuth::hasherRoundTrip()
{
    const QString stored = PasswordHasher::hash(QStringLiteral("1234"));
    QVERIFY(stored.startsWith(QStringLiteral("pbkdf2-sha256$")));
    QVERIFY(!stored.contains(QStringLiteral("1234")));

    QVERIFY(PasswordHasher::verify(QStringLiteral("1234"), stored));
    QVERIFY(!PasswordHasher::verify(QStringLiteral("1235"), stored));
    QVERIFY(!PasswordHasher::verify(QStringLiteral("1234"), QStringLiteral("corrompu")));

    // Sels aléatoires : deux hachages du même PIN diffèrent
    QVERIFY(PasswordHasher::hash(QStringLiteral("1234")) != stored);
}

void TestAuth::firstRunSetup()
{
    // Base vierge -> création du compte Gérant obligatoire (RG-01.a)
    QVERIFY(m_auth->needsSetup());
    QVERIFY(!m_auth->authenticated());

    QVERIFY(m_auth->setupManager(QStringLiteral("Sami"),
                                 QStringLiteral("1234"),
                                 QStringLiteral("1234")));
    QVERIFY(m_auth->authenticated());
    QCOMPARE(m_auth->currentRole(), QStringLiteral("manager"));
    QCOMPARE(m_auth->currentUserName(), QStringLiteral("Sami"));
    QVERIFY(!m_auth->needsSetup());
    m_managerId = m_auth->currentUserId();

    // Un second setup est refusé
    QVERIFY(!m_auth->setupManager(QStringLiteral("Autre"),
                                  QStringLiteral("9999"),
                                  QStringLiteral("9999")));
}

void TestAuth::loginAndLogout()
{
    m_auth->logout();
    QVERIFY(!m_auth->authenticated());

    QVERIFY(!m_auth->login(m_managerId, QStringLiteral("0000")));
    QVERIFY(!m_auth->authenticated());

    QVERIFY(m_auth->login(m_managerId, QStringLiteral("1234")));
    QVERIFY(m_auth->authenticated());
    QCOMPARE(m_auth->currentUserId(), m_managerId);
    m_auth->logout();
}

void TestAuth::invalidPinsRejectedAtSetup()
{
    QTemporaryDir dir;
    DatabaseManager db(dir.filePath(QStringLiteral("fresh.db")),
                       QStringLiteral("tst_auth_fresh"));
    QVERIFY(db.open().isOk());
    SqliteUserRepository users(db.connectionName());
    AuthController auth(users);

    // PIN trop court, non numérique, confirmation différente, nom vide
    QVERIFY(!auth.setupManager(QStringLiteral("X"), QStringLiteral("12"),
                               QStringLiteral("12")));
    QVERIFY(!auth.setupManager(QStringLiteral("X"), QStringLiteral("abcd"),
                               QStringLiteral("abcd")));
    QVERIFY(!auth.setupManager(QStringLiteral("X"), QStringLiteral("1234"),
                               QStringLiteral("1235")));
    QVERIFY(!auth.setupManager(QStringLiteral("  "), QStringLiteral("1234"),
                               QStringLiteral("1234")));
    QVERIFY(auth.needsSetup());
}

void TestAuth::lockoutAfterFiveFailures()
{
    // F01-05 : verrouillage après 5 PIN erronés
    QSignalSpy failures(m_auth, &AuthController::loginFailed);

    for (int attempt = 0; attempt < AuthController::kMaxAttempts; ++attempt)
        QVERIFY(!m_auth->login(m_managerId, QStringLiteral("0000")));

    // Même le bon PIN est refusé une fois verrouillé
    QVERIFY(!m_auth->login(m_managerId, QStringLiteral("1234")));
    QVERIFY(!m_auth->authenticated());
    QCOMPARE(failures.count(), AuthController::kMaxAttempts + 1);
    QVERIFY(failures.last().first().toString().contains(QStringLiteral("verrouillé")));
}

QTEST_GUILESS_MAIN(TestAuth)
#include "tst_auth.moc"
