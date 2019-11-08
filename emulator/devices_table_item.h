#ifndef DEVICESTABLEITEM_H
#define DEVICESTABLEITEM_H

#include <QObject>
#include <QVector>
#include <QVariant>
#include <QModelIndex>
#include <typeinfo>

class DevicesTableItem : public QObject
{
    Q_OBJECT

    QVector<DevicesTableItem*> child_;
    DevicesTableItem* parent_;
protected:
    QObject* itemData_;

    int get_child_count() const;
    DevicesTableItem* get_child(int row) const;
public:
    DevicesTableItem(QObject* data, const QVector<DevicesTableItem*>& children = QVector<DevicesTableItem*>(), DevicesTableItem* parent = nullptr);
    DevicesTableItem(QObject* data, DevicesTableItem* parent = nullptr);
    explicit DevicesTableItem(DevicesTableItem* parent = nullptr);
    virtual ~DevicesTableItem() = default;

    virtual QVariant data(const QModelIndex& index, int role) const = 0;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;

    QObject* item() const;

    DevicesTableItem* parent() const;
    virtual DevicesTableItem* child(int row) const;

    bool hasChild() const;
    virtual int childCount() const;
    void appendChild(DevicesTableItem* item);

    int row() const;

    enum Column
    {
        UNIT_TYPE = 0,
        UNIT_NAME,
        UNIT_VALUE
    };
};

#endif // DEVICESTABLEITEM_H
