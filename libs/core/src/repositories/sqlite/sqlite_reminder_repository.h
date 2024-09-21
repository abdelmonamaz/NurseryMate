#pragma once

#include "repositories/ireminder_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteReminderRepository : public IReminderRepository
{
public:
    explicit SqliteReminderRepository(QString connectionName);

    Result<QList<ReminderRow>> pending() override;
    Result<int> add(const QString& label, const QString& dueDate,
                    int batchId, int userId) override;
    Result<void> markDone(int reminderId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
