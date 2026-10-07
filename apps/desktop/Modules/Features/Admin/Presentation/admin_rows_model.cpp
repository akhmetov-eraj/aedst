#include "Presentation/admin_rows_model.hpp"

#include <utility>

namespace aedst::feature::admin {

AdminRowsModel::AdminRowsModel(QList<QByteArray> columns, QObject* parent)
    : QAbstractListModel(parent), columns_(std::move(columns)) {}

int AdminRowsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant AdminRowsModel::data(const QModelIndex& index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const int column = role - (Qt::UserRole + 1);
    const QVariantList& row = rows_.at(index.row());
    return column >= 0 && column < row.size() ? row.at(column) : QVariant();
}

QHash<int, QByteArray> AdminRowsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    for (qsizetype i = 0; i < columns_.size(); ++i) {
        roles.insert(Qt::UserRole + 1 + static_cast<int>(i), columns_.at(i));
    }
    return roles;
}

void AdminRowsModel::setRows(QList<QVariantList> rows) {
    const bool countChanges = rows.size() != rows_.size();
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
    if (countChanges) {
        emit countChanged();
    }
}

void AdminRowsModel::appendRows(QList<QVariantList> rows) {
    if (rows.isEmpty()) {
        return;
    }
    const int first = static_cast<int>(rows_.size());
    beginInsertRows({}, first, first + static_cast<int>(rows.size()) - 1);
    rows_.append(std::move(rows));
    endInsertRows();
    emit countChanged();
}

} // namespace aedst::feature::admin
