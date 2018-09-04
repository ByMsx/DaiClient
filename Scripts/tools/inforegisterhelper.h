#ifndef INFOREGISTERHELPER_H
#define INFOREGISTERHELPER_H

#include "automationhelper.h"

namespace Dai {

class InfoRegisterHelper : public AutomationHelperItem
{
    Q_OBJECT
    Q_PROPERTY(int size READ size)
public:
    explicit InfoRegisterHelper(ItemGroup* group, uint type, uint info_type, QObject *parent = 0);

    int size() const;
public slots:
    void init(InfoRegisterHelper* other = nullptr);
    QVariant info(DeviceItem* item) const;

    DeviceItem* wndItem(uint i) const;
    DeviceItem* infoItem(uint i) const;
private:
    DeviceItem* getItem(uint i, bool getWnd = true) const;

    uint m_info_type;
    std::map<DeviceItem*, DeviceItem*> m_items;
};

} // namespace Dai

#endif // INFOREGISTERHELPER_H
