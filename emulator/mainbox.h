#ifndef MAINBOX_H
#define MAINBOX_H

#include <vector>

#include <QFrame>
#include <QModbusServer>

#include <Dai/deviceitem.h>

class QHBoxLayout;
class QCheckBox;
class QTableWidget;
class QDoubleSpinBox;
class QSpinBox;
class MainBox : public QFrame
{
    Q_OBJECT
public:
    explicit MainBox(Dai::ItemTypeManager* mng, Dai::Device* dev, QModbusServer* modbus, QWidget *parent = 0);
    Dai::Device* dev;
    QModbusServer* modbus;

    bool isUsed() const;
private slots:
    void updateWidgets(QModbusDataUnit::RegisterType table, int address, int size);
    void setCoilValue(bool value);
    void setRegisterValue(int value);
    void itemToggled(bool state);
protected:
    typedef Dai::DeviceItem* DevItemPtr;
    typedef std::vector<DevItemPtr> DevItems;

    void setDeviceValue(QModbusDataUnit::RegisterType table, quint16 value, QWidget* w = nullptr);

    Dai::DeviceItem* item0();
    QHBoxLayout* box;
    QCheckBox* checkBox;

    std::map<QModbusDataUnit::RegisterType, DevItems> m_items;

//    DevItems controls;
//    DevItems sensors;

    typedef std::multimap<DevItemPtr, QWidget*> WidgetsMap;
    WidgetsMap widgets;

    Dai::ItemTypeManager* mng;
};

class ModbusBox : public MainBox
{
    Q_OBJECT
public:
    using MainBox::MainBox;
//    ModbusBox(Dai::Device *dev, QModbusServer* modbus, QWidget *parent = 0);
private slots:
//    void setDeviceValue(bool b);
//    void valueChanged();
private:
    QCheckBox* cb;
};


#endif // MAINBOX_H
