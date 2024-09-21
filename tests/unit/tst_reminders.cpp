#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_reminder_repository.h"

#include <QDate>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

// Rappels d'entretien (F08-04) : échus d'abord, fait = disparu, saisies
// invalides refusées.
class TestReminders : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void overdueFirstThenDue();
    void markDoneRemoves();
    void invalidRejected();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteReminderRepository* m_reminders = nullptr;
};

void TestReminders::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("rem.db")),
                               QStringLiteral("tst_reminders"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);
    m_reminders = new SqliteReminderRepository(m_db->connectionName());
}

void TestReminders::overdueFirstThenDue()
{
    const QDate today = QDate::currentDate();
    QVERIFY(m_reminders->add(QStringLiteral("fertiliser Serre 1"),
                             today.addDays(15).toString(Qt::ISODate), 0, 0)
                .isOk());
    QVERIFY(m_reminders->add(QStringLiteral("traiter les agrumes"),
                             today.addDays(-3).toString(Qt::ISODate), 0, 0)
                .isOk());
    QVERIFY(m_reminders->add(QStringLiteral("tailler les rosiers"),
                             today.addDays(2).toString(Qt::ISODate), 0, 0)
                .isOk());

    const auto pending = m_reminders->pending();
    QVERIFY(pending.isOk());
    QCOMPARE(pending.value().size(), 3);
    // Échu d'abord, puis par échéance croissante
    QCOMPARE(pending.value().at(0).label, QStringLiteral("traiter les agrumes"));
    QVERIFY(pending.value().at(0).overdue);
    QCOMPARE(pending.value().at(1).label, QStringLiteral("tailler les rosiers"));
    QVERIFY(!pending.value().at(1).overdue);
    QCOMPARE(pending.value().at(2).label, QStringLiteral("fertiliser Serre 1"));
}

void TestReminders::markDoneRemoves()
{
    const int overdueId = m_reminders->pending().value().first().id;
    QVERIFY(m_reminders->markDone(overdueId).isOk());
    const auto pending = m_reminders->pending();
    QCOMPARE(pending.value().size(), 2);
    for (const ReminderRow& row : pending.value())
        QVERIFY(row.id != overdueId);
}

void TestReminders::invalidRejected()
{
    QVERIFY(!m_reminders->add(QStringLiteral("   "),
                              QStringLiteral("2026-08-01"), 0, 0).isOk());
    QVERIFY(!m_reminders->add(QStringLiteral("x"),
                              QStringLiteral("pas-une-date"), 0, 0).isOk());
    QCOMPARE(m_reminders->pending().value().size(), 2);
}

QTEST_GUILESS_MAIN(TestReminders)
#include "tst_reminders.moc"
