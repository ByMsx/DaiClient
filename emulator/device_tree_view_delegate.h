#ifndef BUTTONCOLUMNDELEGATE_H
#define BUTTONCOLUMNDELEGATE_H

#include <QObject>
#include <QStyledItemDelegate>
#include <QTreeView>

class DeviceTreeViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit DeviceTreeViewDelegate(QTreeView* parent = nullptr);
    ~DeviceTreeViewDelegate() override = default;

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    static bool is_our_cell(const QModelIndex& index);
};

#endif // BUTTONCOLUMNDELEGATE_H
