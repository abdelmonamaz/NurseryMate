#include "controllers/customer_controller.h"

#include <QLocale>

namespace nursera {
namespace {

PaymentMethod methodFromString(const QString& text)
{
    if (text == QLatin1String("cheque")) return PaymentMethod::Cheque;
    if (text == QLatin1String("transfer")) return PaymentMethod::Transfer;
    return PaymentMethod::Cash;
}

} // namespace

CustomerController::CustomerController(ICustomerRepository& customers,
                                       QObject* parent)
    : QObject(parent)
    , m_customers(customers)
{
}

void CustomerController::setSearchTerm(const QString& term)
{
    if (m_searchTerm == term)
        return;
    m_searchTerm = term;
    emit searchTermChanged();
    refresh();
}

void CustomerController::setShowInactive(bool show)
{
    if (m_showInactive == show)
        return;
    m_showInactive = show;
    emit showInactiveChanged();
    refresh();
}

void CustomerController::refresh()
{
    const auto found = m_customers.search(m_searchTerm, m_showInactive);
    if (!found) {
        emit errorOccurred(found.error().message);
        return;
    }

    const QLocale locale;
    m_rows.clear();
    for (const CustomerRow& row : found.value()) {
        m_rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("name"), row.name},
            {QStringLiteral("phone"), row.phone},
            {QStringLiteral("professional"),
             row.kind == CustomerKind::Professional},
            {QStringLiteral("active"), row.active},
            {QStringLiteral("balanceDisplay"), row.balance.toDisplayString(locale)},
            {QStringLiteral("hasDebt"), row.balance.millimes() > 0},
            {QStringLiteral("creditLimitDisplay"),
             row.creditLimitMillimes >= 0
                 ? Money::fromMillimes(row.creditLimitMillimes).toDisplayString(locale)
                 : QString()},
        });
    }
    emit refreshed();
}

bool CustomerController::createCustomer(const QVariantMap& data)
{
    Customer customer;
    customer.name = data.value(QStringLiteral("name")).toString().trimmed();
    if (customer.name.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }
    customer.kind = data.value(QStringLiteral("kind")).toString()
                        == QLatin1String("professional")
        ? CustomerKind::Professional
        : CustomerKind::Individual;
    customer.phone = data.value(QStringLiteral("phone")).toString().trimmed();

    // Détection de doublon téléphone (F06-04)
    if (const auto exists = m_customers.phoneExists(customer.phone);
        exists.isOk() && exists.value()) {
        emit errorOccurred(tr("Un client actif a déjà ce numéro de téléphone."));
        return false;
    }

    const QString limitText =
        data.value(QStringLiteral("creditLimit")).toString().trimmed();
    if (!limitText.isEmpty()) {
        const qint64 limit = Money::parseMillimes(limitText);
        if (limit < 0) {
            emit errorOccurred(tr("Plafond invalide — format attendu : 500"));
            return false;
        }
        customer.creditLimitMillimes = limit;
    }

    const auto created = m_customers.insert(customer);
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit customerCreated(created.value());
    return true;
}

QVariantMap CustomerController::customerDetail(int customerId)
{
    const auto found = m_customers.byId(customerId);
    if (!found) {
        emit errorOccurred(found.error().message);
        return {};
    }
    const Customer& customer = found.value();
    const QLocale locale;
    return QVariantMap{
        {QStringLiteral("id"), customer.id},
        {QStringLiteral("name"), customer.name},
        {QStringLiteral("kind"),
         customer.kind == CustomerKind::Professional
             ? QStringLiteral("professional")
             : QStringLiteral("individual")},
        {QStringLiteral("phone"), customer.phone},
        {QStringLiteral("phone2"), customer.phone2},
        {QStringLiteral("email"), customer.email},
        {QStringLiteral("address"), customer.address},
        {QStringLiteral("taxId"), customer.taxId},
        {QStringLiteral("notes"), customer.notes},
        {QStringLiteral("creditLimit"),
         customer.creditLimitMillimes >= 0
             ? QString::number(customer.creditLimitMillimes / 1000.0, 'f', 3)
                   .replace(QLatin1Char('.'), QLatin1Char(','))
             : QString()},
        {QStringLiteral("active"), customer.active},
    };
}

bool CustomerController::updateCustomer(const QVariantMap& data)
{
    const int customerId = data.value(QStringLiteral("id")).toInt();
    const auto found = m_customers.byId(customerId);
    if (customerId <= 0 || !found) {
        emit errorOccurred(tr("Client introuvable."));
        return false;
    }

    Customer customer = found.value();
    customer.name = data.value(QStringLiteral("name")).toString().trimmed();
    if (customer.name.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }
    customer.kind = data.value(QStringLiteral("kind")).toString()
                        == QLatin1String("professional")
        ? CustomerKind::Professional
        : CustomerKind::Individual;

    // Doublon téléphone : seulement si le numéro change (F06-04).
    const QString phone = data.value(QStringLiteral("phone")).toString().trimmed();
    if (phone != customer.phone && !phone.isEmpty()) {
        if (const auto exists = m_customers.phoneExists(phone);
            exists.isOk() && exists.value()) {
            emit errorOccurred(tr("Un client actif a déjà ce numéro de téléphone."));
            return false;
        }
    }
    customer.phone = phone;
    customer.phone2 = data.value(QStringLiteral("phone2")).toString().trimmed();
    customer.email = data.value(QStringLiteral("email")).toString().trimmed();
    customer.address = data.value(QStringLiteral("address")).toString().trimmed();
    customer.taxId = data.value(QStringLiteral("taxId")).toString().trimmed();
    customer.notes = data.value(QStringLiteral("notes")).toString().trimmed();

    const QString limitText =
        data.value(QStringLiteral("creditLimit")).toString().trimmed();
    if (limitText.isEmpty()) {
        customer.creditLimitMillimes = -1;
    } else {
        const qint64 limit = Money::parseMillimes(limitText);
        if (limit < 0) {
            emit errorOccurred(tr("Plafond invalide — format attendu : 500"));
            return false;
        }
        customer.creditLimitMillimes = limit;
    }

    const auto updated = m_customers.update(customer);
    if (!updated) {
        emit errorOccurred(updated.error().message);
        return false;
    }
    refresh();
    emit customerUpdated();
    return true;
}

bool CustomerController::setCustomerActive(int customerId, bool active)
{
    const auto changed = m_customers.setActive(customerId, active);
    if (!changed) {
        emit errorOccurred(changed.error().message);
        return false;
    }
    refresh();
    emit customerUpdated();
    return true;
}

bool CustomerController::isDeletable(int customerId)
{
    const auto referenced = m_customers.isReferenced(customerId);
    return referenced.isOk() && !referenced.value();
}

bool CustomerController::deleteCustomer(int customerId)
{
    const auto removed = m_customers.remove(customerId);
    if (!removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    refresh();
    emit customerUpdated();
    return true;
}

bool CustomerController::recordPayment(const QVariantMap& data)
{
    const int customerId = data.value(QStringLiteral("customerId")).toInt();
    const qint64 amount =
        Money::parseMillimes(data.value(QStringLiteral("amount")).toString());
    if (amount <= 0) {
        emit errorOccurred(tr("Montant invalide — format attendu : 50,000"));
        return false;
    }

    const auto recorded = m_customers.recordPayment(
        customerId, Money::fromMillimes(amount),
        methodFromString(data.value(QStringLiteral("method")).toString()),
        data.value(QStringLiteral("chequeNumber")).toString().trimmed(),
        data.value(QStringLiteral("chequeBank")).toString().trimmed(),
        m_userIdProvider ? m_userIdProvider() : 0);
    if (!recorded) {
        emit errorOccurred(recorded.error().message);
        return false;
    }
    refresh();
    emit paymentRecorded();
    return true;
}

} // namespace nursera
