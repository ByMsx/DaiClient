#ifndef MAINBOX_H
#define MAINBOX_H

#include <vector>

#include <QFrame>
#include <QModbusServer>

#include <Dai/deviceitem.h>
#include <Dai/typemanager/typemanager.h>

class QHBoxLayout;
class QCheckBox;
class QTableWidget;
class QDoubleSpinBox;
class QSpinBox;
class MainBox : public QFrame
{
    Q_OBJECT
public:
    explicit MainBox(Dai::Item_Type_Manager* mng, Dai::Device* dev, QModbusServer* modbus, QWidget *parent = 0);
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

    Dai::Item_Type_Manager* mng;
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

/*
class WindowBox : public MainBox
{
    Q_OBJECT
public:
    WindowBox(Dai::Device *dev, QWidget *parent = 0);
private slots:
    void setDeviceValue(bool b);
    void valueChanged();
private:
    QCheckBox* cb(int column);
    QTableWidget* tbl;
};

class TemperatureBox : public MainBox
{
    Q_OBJECT
public:
    TemperatureBox(Dai::Device *dev, QWidget *parent = 0);
private slots:
    void setDeviceValue();
    void valueChanged();
private:
    struct Item {
        QCheckBox* cb;
        QDoubleSpinBox* spin;
    };

    std::map<const Dai::Prt::DeviceItem*, Item> items;
};

class LightBox : public MainBox
{
    Q_OBJECT
public:
    LightBox(Dai::Device *dev, QWidget *parent = 0);
private slots:
    void valueChanged();
private:
    QSpinBox* spin;
};

class OnOffBox : public MainBox
{
    Q_OBJECT
public:
    OnOffBox(Dai::Device *dev, QWidget *parent = 0);
private slots:
    void setDeviceValue(bool b);
    void valueChanged();
private:
    QCheckBox* cb;
};*/

#endif // MAINBOX_H
