#ifndef UNITS_TABLE_DELEGATE_H
#define UNITS_TABLE_DELEGATE_H

#include <QStyledItemDelegate>

class Units_Table_Delegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit Units_Table_Delegate(QObject *parent = 0);

    // QAbstractItemDelegate interface
public:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // UNITS_TABLE_DELEGATE_H
