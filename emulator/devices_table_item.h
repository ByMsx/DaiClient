#ifndef DEVICESTABLEITEM_H
#define DEVICESTABLEITEM_H

#include <QObject>
#include <QVector>
#include <QVariant>
#include <QModelIndex>
#include <typeinfo>

class DevicesTableItem
{
    QVector<DevicesTableItem*> child_;
    DevicesTableItem* parent_;
protected:
    QObject* itemData_;
public:
    DevicesTableItem(QObject* data, const QVector<DevicesTableItem*>& children = QVector<DevicesTableItem*>(), DevicesTableItem* parent = nullptr);
    DevicesTableItem(QObject* data, DevicesTableItem* parent = nullptr);
    explicit DevicesTableItem(DevicesTableItem* parent = nullptr);

    virtual ~DevicesTableItem() = default;

    virtual QVariant data(const QModelIndex& index, int role) const = 0;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;

    DevicesTableItem* parent() const;
    DevicesTableItem* child(int row) const;

    bool hasChild() const;
    int childCount() const;
    int row() const;
    void appendChild(DevicesTableItem* item);
    QObject* item() const;
};

#endif // DEVICESTABLEITEM_H
