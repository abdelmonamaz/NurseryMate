#include "controllers/reminder_controller.h"

#include <QDate>
#include <QVariantMap>

namespace nursera {

ReminderController::ReminderController(IReminderRepository& reminders,
                                       QObject* parent)
    : QObject(parent)
    , m_repository(reminders)
{
}

void ReminderController::refresh()
{
    const auto pending = m_repository.pending();
    if (!pending) {
        emit errorOccurred(pending.error().message);
        return;
    }
    m_reminders.clear();
    m_overdueCount = 0;
    for (const ReminderRow& row : pending.value()) {
        if (row.overdue)
            ++m_overdueCount;
        m_reminders.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("dueDate"), row.dueDate},
            {QStringLiteral("batchNumber"), row.batchNumber},
            {QStringLiteral("overdue"), row.overdue},
        });
    }
    emit refreshed();
}

bool ReminderController::addReminder(const QString& label,
                                     const QString& dueDate, int batchId)
{
    const auto added = m_repository.add(
        label, dueDate, batchId, m_userIdProvider ? m_userIdProvider() : 0);
    if (!added) {
        emit errorOccurred(added.error().message);
        return false;
    }
    refresh();
    emit reminderSaved();
    return true;
}

bool ReminderController::addReminderInDays(const QString& label, int days,
                                           int batchId)
{
    return addReminder(
        label,
        QDate::currentDate().addDays(qMax(1, days)).toString(Qt::ISODate),
        batchId);
}

bool ReminderController::markDone(int reminderId)
{
    const auto done = m_repository.markDone(reminderId);
    if (!done) {
        emit errorOccurred(done.error().message);
        return false;
    }
    refresh();
    return true;
}

} // namespace nursera
