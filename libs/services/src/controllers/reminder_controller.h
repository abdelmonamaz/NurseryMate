#pragma once

#include "repositories/ireminder_repository.h"

#include <QObject>
#include <QVariantList>

#include <functional>

namespace nursera {

// Rappels d'entretien (F08-04) : planifiés depuis une intervention de lot
// ou librement, affichés au tableau de bord (échus d'abord).
class ReminderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList reminders READ reminders NOTIFY refreshed)
    Q_PROPERTY(int overdueCount READ overdueCount NOTIFY refreshed)

public:
    explicit ReminderController(IReminderRepository& reminders,
                                QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    QVariantList reminders() const { return m_reminders; }
    int overdueCount() const { return m_overdueCount; }

    Q_INVOKABLE void refresh();
    // dueDate ISO (AAAA-MM-JJ) ; batchId 0 = rappel libre.
    Q_INVOKABLE bool addReminder(const QString& label, const QString& dueDate,
                                 int batchId);
    // Raccourci « dans N jours » (depuis le dialogue d'intervention).
    Q_INVOKABLE bool addReminderInDays(const QString& label, int days,
                                       int batchId);
    Q_INVOKABLE bool markDone(int reminderId);

signals:
    void refreshed();
    void reminderSaved();
    void errorOccurred(const QString& message);

private:
    IReminderRepository& m_repository;
    QVariantList m_reminders;
    int m_overdueCount = 0;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
