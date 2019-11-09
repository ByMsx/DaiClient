#ifndef BUTTONCOLUMNDELEGATE_H
#define BUTTONCOLUMNDELEGATE_H

#include <QObject>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QPushButton>

class DeviceTreeViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit DeviceTreeViewDelegate(QTreeView* parent = nullptr);
    ~DeviceTreeViewDelegate() override = default;

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
private:
    static bool is_input_or_holding_register_value_cell(const QModelIndex& index);
    static bool is_device_value_cell(const QModelIndex& index);

    QPushButton* btn_;
    QTreeView* parent_widget_;
};

#endif // BUTTONCOLUMNDELEGATE_H
