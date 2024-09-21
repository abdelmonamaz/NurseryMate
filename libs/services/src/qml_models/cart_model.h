#pragma once

#include "models/sale.h"

#include <QAbstractListModel>
#include <QList>

namespace nursera {

// Panier de la caisse (M04). Le brouillon est restauré après un crash
// par le controller (RT-06 — persistance à venir avec les paramètres).
class CartModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        VariantIdRole = Qt::UserRole + 1,
        LabelRole,
        QtyRole,
        UnitPriceRole,    // affichage formaté "12,500 DT"
        UnitPriceRawRole, // édition "12,500"
        LineTotalRole,    // affichage formaté
        ManualPriceRole,  // prix négocié à la main
    };

    explicit CartModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Ajoute ou incrémente si la variante est déjà au panier.
    void addLine(const SaleLine& line);
    void setQty(int row, int qty);
    // Prix unitaire négocié à la main (F04-01).
    void setUnitPrice(int row, Money price);
    void removeAt(int row);
    void clear();

    // Re-tarifie les lignes selon le type de client (RG-04.c) ;
    // ignore les lignes à prix négocié.
    void repriceForPro(bool pro);

    const QList<SaleLine>& lines() const { return m_lines; }
    void setLines(QList<SaleLine> lines); // restauration d'un panier en attente
    Money subtotal() const;
    int itemCount() const;

private:
    QList<SaleLine> m_lines;
};

} // namespace nursera
