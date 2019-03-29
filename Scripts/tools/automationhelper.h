#ifndef AUTOMATIONHELPER_H
#define AUTOMATIONHELPER_H

#include <vector>

#include <QScriptValue>

#include <Dai/section.h>

namespace Dai {

class AutomationHelperItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(ItemGroup* group READ group)
    Q_PROPERTY(uint type READ type WRITE setType)
    Q_PROPERTY(QScriptValue data READ data WRITE setData)
public:
    AutomationHelperItem(ItemGroup* group = nullptr, uint type = 0, const QScriptValue &value = QScriptValue(), QObject* parent = nullptr);

    ItemGroup* group() const;
    uint type() const;

    QScriptValue data() const;
    void setData(const QScriptValue &value);

public slots:
    virtual void init() {}
    void setType(uint type);
    void writeToControl(const QVariant &raw_data, uint mode = 2);
private:
    ItemGroup* m_group;
    uint m_type;
    QScriptValue m_data;

    friend class AutomationHelper;
};

class AutomationHelper : public QObject {
    Q_OBJECT
public:
    AutomationHelper(const QScriptValue &type = QScriptValue(), QObject* parent = nullptr);
    void clear();
public slots:
    void init();
    void addGroup(AutomationHelperItem *helper);
    void addGroup(ItemGroup* group, const QScriptValue &value);
    AutomationHelperItem* item(ItemGroup* group) const;
    AutomationHelperItem* item(uint group_id) const;
private:
    std::vector<AutomationHelperItem*> m_items;
    uint m_type;
};

} // namespace Dai

Q_DECLARE_METATYPE(Dai::AutomationHelper*)
Q_DECLARE_METATYPE(Dai::AutomationHelperItem*)

#endif // AUTOMATIONHELPER_H
