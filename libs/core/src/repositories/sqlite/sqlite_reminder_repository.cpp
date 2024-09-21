#include "repositories/sqlite/sqlite_reminder_repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {

SqliteReminderRepository::SqliteReminderRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<QList<ReminderRow>> SqliteReminderRepository::pending()
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT r.id, r.label, r.due_date, COALESCE(b.number, ''), "
            "       date(r.due_date) < date('now') "
            "FROM reminders r LEFT JOIN batches b ON b.id = r.batch_id "
            "WHERE r.done = 0 "
            "ORDER BY date(r.due_date) < date('now') DESC, r.due_date")))
        return Result<QList<ReminderRow>>::fail(QStringLiteral("reminder.list"),
                                                query.lastError().text());
    QList<ReminderRow> rows;
    while (query.next()) {
        ReminderRow row;
        row.id = query.value(0).toInt();
        row.label = query.value(1).toString();
        row.dueDate = query.value(2).toString();
        row.batchNumber = query.value(3).toString();
        row.overdue = query.value(4).toBool();
        rows.append(row);
    }
    return Result<QList<ReminderRow>>::ok(std::move(rows));
}

Result<int> SqliteReminderRepository::add(const QString& label,
                                          const QString& dueDate,
                                          int batchId, int userId)
{
    if (label.trimmed().isEmpty())
        return Result<int>::fail(QStringLiteral("reminder.label"),
                                 QStringLiteral("Le libellé est obligatoire."));
    if (!QDate::fromString(dueDate, Qt::ISODate).isValid())
        return Result<int>::fail(QStringLiteral("reminder.date"),
                                 QStringLiteral("Échéance invalide "
                                                "(AAAA-MM-JJ)."));

    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO reminders (uuid, label, due_date, batch_id, user_id) "
        "VALUES (:uuid, :label, :due, :batch, :user)"));
    insert.bindValue(QStringLiteral(":uuid"),
                     QUuid::createUuid().toString(QUuid::WithoutBraces));
    insert.bindValue(QStringLiteral(":label"), label.trimmed());
    insert.bindValue(QStringLiteral(":due"), dueDate);
    insert.bindValue(QStringLiteral(":batch"),
                     batchId > 0 ? QVariant(batchId) : QVariant());
    insert.bindValue(QStringLiteral(":user"),
                     userId > 0 ? QVariant(userId) : QVariant());
    if (!insert.exec())
        return Result<int>::fail(QStringLiteral("reminder.insert"),
                                 insert.lastError().text());
    return Result<int>::ok(insert.lastInsertId().toInt());
}

Result<void> SqliteReminderRepository::markDone(int reminderId)
{
    QSqlQuery update(db());
    update.prepare(QStringLiteral(
        "UPDATE reminders SET done = 1 WHERE id = :id"));
    update.bindValue(QStringLiteral(":id"), reminderId);
    if (!update.exec())
        return Result<void>::fail(QStringLiteral("reminder.done"),
                                  update.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
