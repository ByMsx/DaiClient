#include <QDebug>

#include <Dai/device.h>

#include "inforegisterhelper.h"

namespace Dai {

InfoRegisterHelper::InfoRegisterHelper(ItemGroup *group, uint type, uint info_type, QObject *parent) :
    AutomationHelperItem(group, type, QScriptValue(), parent),
    m_info_type(info_type)
{
}

int InfoRegisterHelper::size() const { return m_items.size(); }

void InfoRegisterHelper::init(InfoRegisterHelper* other)
{
    m_items.clear();

    if (other)
        m_items = other->m_items;

    DeviceItems infos;

    for (auto& item: group()->items())
    {
        if (item->type_id() == type())
            m_items.emplace(item, nullptr);
        else if (item->type_id() == m_info_type)
            infos.push_back(item);
    }

    for (auto& it: m_items)
    {
        for (auto item: infos)
        {
            if (    it.first->device()->params() == item->device()->params() &&
                    it.first->params() == item->params() )
            {
                it.second = item;
                break;
            }
        }
    }
}

QVariant InfoRegisterHelper::info(DeviceItem *item) const
{
    auto it = m_items.find(item);
    if (it != m_items.cend() && it->second)
        return it->second->value();
    return QVariant();
}

DeviceItem *InfoRegisterHelper::wndItem(uint i) const
{
    return getItem(i);
}

DeviceItem *InfoRegisterHelper::infoItem(uint i) const
{
    return getItem(i, false);
}

DeviceItem *InfoRegisterHelper::getItem(uint i, bool getWnd) const
{
    if (i < m_items.size())
    {
        auto it = m_items.cbegin();
        std::advance(it, i);
        return getWnd ? it->first : it->second;
    }

    return nullptr;
}

} // namespace Dai
