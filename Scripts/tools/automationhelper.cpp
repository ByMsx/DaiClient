#include <QDebug>
#include <QScriptEngine>

#include "automationhelper.h"

namespace Dai {

AutomationHelperItem::AutomationHelperItem(ItemGroup *group, uint type, const QScriptValue &value, QObject *parent) :
    QObject(parent), m_group(group), m_type(type), m_data(value)
{    /*
    if (value.isValid())
    {
        auto addProp = [this](const QString& name, const QScriptValue &value) {
//            qDebug() << "addProp" << name << value.toString();
            setProperty(qPrintable(name), value.toVariant());
        };

        QString length = "length";

        if (value.isArray())
        {
            auto len_prop = value.property(length);
            addProp(length, len_prop);

            int len = len_prop.toInt32();
            for (int i = 0; i < len; ++i) {
                auto n = QString::number(i);
                addProp(n, value.property(n));
            }
        }
        else if (value.isObject())
        {
            if (value.isFunction()) {
                QString name = value.property("name").toString();
                if (!name.isEmpty())
                    addProp(name, value);
            }
            else
            {
                addProp("data", value);

                / *
                 * Вариант когда все свойства json перебираются и пирсваиваются в property этого объекта
                QString temp_var_name = "AutomationHelperItemPassedObject";
                value.engine()->globalObject().setProperty(temp_var_name, value);
                auto args = value.engine()->evaluate("Object.keys(" + temp_var_name + ")");
                value.engine()->globalObject().setProperty(temp_var_name, value.engine()->undefinedValue());

                if (args.isArray())
                {
                    uint len = args.property(length).toUInt32();
                    for (uint i = 0; i < len; ++i)
                    {
                        auto name = args.property(QString::number(i)).toString();
                        addProp(name, value.property(name));
                    }
                }
                * /
            }
        }
        else
            qWarning() << "AutomationHelperItem unsupported value type" << value.toVariant();
    }*/
}

ItemGroup *AutomationHelperItem::group() const { return m_group; }

uint AutomationHelperItem::type() const { return m_type; }
void AutomationHelperItem::setType(uint type) { m_type = type; }

void AutomationHelperItem::writeToControl(const QVariant &raw_data, uint mode)
{
    group()->write_to_control(m_type, raw_data, mode);
}

QScriptValue AutomationHelperItem::data() const { return m_data; }

void AutomationHelperItem::setData(const QScriptValue &value) { m_data = value; }

AutomationHelper::AutomationHelper(const QScriptValue &type, QObject *parent) :
    QObject(parent),
    m_type(type.toUInt32())
{
    m_items.reserve(3);
}

void AutomationHelper::clear()
{
    for (AutomationHelperItem* it: m_items)
        it->deleteLater();
    m_items.clear();
}

void AutomationHelper::init()
{
    for (AutomationHelperItem* it: m_items)
        it->init();
}

void AutomationHelper::addGroup(AutomationHelperItem *helper)
{
    helper->setParent(this);
    if (!helper->type() && m_type)
        helper->setType(m_type);
    m_items.push_back(helper);
}

void AutomationHelper::addGroup(ItemGroup *group, const QScriptValue &value)
{
    addGroup(new AutomationHelperItem(group, m_type, value));
}

AutomationHelperItem *AutomationHelper::item(ItemGroup *group) const
{
    for (AutomationHelperItem* it: m_items)
        if (it->group() == group)
            return it;
    return nullptr;
}

AutomationHelperItem *AutomationHelper::item(uint group_id) const
{
    for (AutomationHelperItem* it: m_items)
        if (it->group()->id() == group_id)
            return it;
    return nullptr;
}

} // namespace Dai
