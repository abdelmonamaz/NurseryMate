#pragma once

#include <QString>

#include <optional>
#include <utility>

namespace nursera {

// Erreur typée : code stable (logs, tests) + message traduisible (UI).
struct Error
{
    QString code;
    QString message;
};

// Retour des services métier : valeur ou erreur, jamais d'exception
// à travers la frontière QML (doc 02 §2.2).
template <typename T>
class Result
{
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result fail(Error error) { return Result(std::move(error)); }
    static Result fail(QString code, QString message)
    {
        return Result(Error{std::move(code), std::move(message)});
    }

    bool isOk() const { return m_value.has_value(); }
    explicit operator bool() const { return isOk(); }

    const T& value() const { return *m_value; }
    T& value() { return *m_value; }
    const Error& error() const { return m_error; }

private:
    explicit Result(T value) : m_value(std::move(value)) {}
    explicit Result(Error error) : m_error(std::move(error)) {}

    std::optional<T> m_value;
    Error m_error;
};

// Spécialisation pour les opérations sans valeur de retour.
template <>
class Result<void>
{
public:
    static Result ok() { return Result(true); }
    static Result fail(Error error) { return Result(std::move(error)); }
    static Result fail(QString code, QString message)
    {
        return Result(Error{std::move(code), std::move(message)});
    }

    bool isOk() const { return m_ok; }
    explicit operator bool() const { return m_ok; }
    const Error& error() const { return m_error; }

private:
    explicit Result(bool ok) : m_ok(ok) {}
    explicit Result(Error error) : m_error(std::move(error)) {}

    bool m_ok = false;
    Error m_error;
};

} // namespace nursera
