#include "repositories/sqlite/sqlite_category_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

Category fromQuery(const QSqlQuery& query)
{
    Category category;
    category.id = query.value(0).toInt();
    category.parentId = query.value(1).toInt(); // NULL -> 0 (racine)
    category.nameFr = query.value(2).toString();
    category.nameAr = query.value(3).toString();
    category.sortOrder = query.value(4).toInt();
    category.active = query.value(5).toBool();
    return category;
}

} // namespace

SqliteCategoryRepository::SqliteCategoryRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<QList<Category>> SqliteCategoryRepository::all(bool includeInactive)
{
    QString sql = QStringLiteral(
        "SELECT id, parent_id, name_fr, name_ar, sort_order, active "
        "FROM categories ");
    if (!includeInactive)
        sql += QStringLiteral("WHERE active = 1 ");
    sql += QStringLiteral("ORDER BY sort_order, name_fr COLLATE NOCASE");

    QSqlQuery query(db());
    if (!query.exec(sql))
        return Result<QList<Category>>::fail(QStringLiteral("category.all"),
                                             query.lastError().text());

    QList<Category> categories;
    while (query.next())
        categories.append(fromQuery(query));
    return Result<QList<Category>>::ok(std::move(categories));
}

Result<int> SqliteCategoryRepository::insert(const Category& category)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO categories (parent_id, name_fr, name_ar, sort_order, active) "
        "VALUES (:parent, :fr, :ar, :sort, :active)"));
    query.bindValue(QStringLiteral(":parent"),
                    category.parentId > 0 ? QVariant(category.parentId) : QVariant());
    query.bindValue(QStringLiteral(":fr"), category.nameFr);
    query.bindValue(QStringLiteral(":ar"), category.nameAr);
    query.bindValue(QStringLiteral(":sort"), category.sortOrder);
    query.bindValue(QStringLiteral(":active"), category.active ? 1 : 0);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("category.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<bool> SqliteCategoryRepository::isReferenced(int categoryId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM products WHERE category_id = :c1) "
        "OR EXISTS(SELECT 1 FROM categories WHERE parent_id = :c2)"));
    query.bindValue(QStringLiteral(":c1"), categoryId);
    query.bindValue(QStringLiteral(":c2"), categoryId);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("category.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteCategoryRepository::remove(int categoryId)
{
    // Garde-fou : jamais de suppression d'un référentiel référencé (norme).
    if (const auto referenced = isReferenced(categoryId); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("category.referenced"),
            QStringLiteral("Cette catégorie est utilisée par des produits "
                           "ou des sous-catégories — désactivez-la."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM categories WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), categoryId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("category.remove"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteCategoryRepository::update(const Category& category)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE categories SET parent_id = :parent, name_fr = :fr, name_ar = :ar, "
        "sort_order = :sort, active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":parent"),
                    category.parentId > 0 ? QVariant(category.parentId) : QVariant());
    query.bindValue(QStringLiteral(":fr"), category.nameFr);
    query.bindValue(QStringLiteral(":ar"), category.nameAr);
    query.bindValue(QStringLiteral(":sort"), category.sortOrder);
    query.bindValue(QStringLiteral(":active"), category.active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), category.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("category.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
