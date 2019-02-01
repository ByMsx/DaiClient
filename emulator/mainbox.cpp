#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QDebug>

#include <QGroupBox>

#include <limits>
#include <iterator>
#include <vector>
#include <map>
#include <cmath>

#include "Dai/typemanager/typemanager.h"
#include "Dai/section.h"
#include "mainbox.h"

using namespace std::placeholders;

MainBox::MainBox(Dai::ItemTypeManager *mng, Dai::Device* dev, QModbusServer *modbus, QWidget *parent) :
    QFrame(parent),
    dev(dev),
    modbus(modbus), mng(mng)
{
    int item_size = dev->items().size();
    if (!item_size)
        return;

    connect(modbus, &QModbusServer::dataWritten, this, &MainBox::updateWidgets);

    for (Dai::DeviceItem* item: dev->items())
        item->setData(0, 0);

    checkBox = new QCheckBox(QString::number(dev->address()), this);
    checkBox->setChecked(true);

    box = new QHBoxLayout;
    box->setMargin(3);
    box->addWidget(checkBox);

    setLayout(box);

    int item_i = 0, threshold = item_size / 2;
    QHBoxLayout *aditHBox1 = nullptr, *aditHBox2 = nullptr;
    if (item_size > 4)
    {
        auto additionalBox = new QVBoxLayout;
        additionalBox->addLayout(aditHBox1 = new QHBoxLayout);
        additionalBox->addLayout(aditHBox2 = new QHBoxLayout);
        box->addLayout(additionalBox, 1);
    }

    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);

//    setTitle(QString::fromStdString(dev->name()));

    auto get_name = [&mng](Dai::DeviceItem* item)
    {
        QString name("%1 (%2)");
        name += item->displayName();
        if (name.length() > 7)
        {
            auto parts = name.split(' ');
            for (QString& str: parts)
                if (str.length() > 4)
                    str.resize(4);
            name = parts.join(' ');
        }
        return name.arg(item->id()).arg(item->unit().toUInt());
    };

    auto addWidget = [&](Dai::DeviceItem* item, QWidget* w)
    {
        auto w_box_lt = new QVBoxLayout;
        w_box_lt->setMargin(0);
        w_box_lt->setSpacing(0);
        w_box_lt->addWidget(w);

        auto w_box = new QGroupBox(get_name(item), this);
        connect(w_box, &QGroupBox::toggled, this, &MainBox::itemToggled);
        w_box->setCheckable(true);
        w_box->setLayout(w_box_lt);

        w->setParent(w_box);

        auto t_box = aditHBox1 ? ++item_i > threshold ? aditHBox2 : aditHBox1 : box;
        t_box->addWidget(w_box);

        widgets.emplace(item, w);
    };

//    for (auto item: controls)
    {
//        auto cb = new QCheckBox("Вкл/Выкл");
//        connect(cb, SIGNAL(clicked(bool)), SLOT(setDeviceValue(bool)));

//        addWidget(item, cb);
//        connect(item0(), SIGNAL(valueChanged()), SLOT(valueChanged()));
//        item0()->setValue(Dai::Prt::wClosed);
    }

//    for (auto item: sensors)
    {
//        auto spin = new QSpinBox;
//        spin->setRange(std::numeric_limits<qint16>::min(),
//                       std::numeric_limits<qint16>::max());

//        addWidget(item, spin);

//        connect(spin, qSpinBoxValueChanged(), std::bind(&Dai::DeviceItem::setValue, item0(), _1));
//        connect(item0(), SIGNAL(valueChanged()), SLOT(valueChanged()));
    }

    std::map<Dai::DeviceItem*,int> val_idx;

    int coils_count = 0, registers_count = 0;

    auto it = dev->items().cbegin();
    for (int i = 0; it != dev->items().cend(); ++i, ++it)
    {
        Dai::DeviceItem* item = *it;
        QWidget* w = nullptr;

        if (    mng->registerType(item->type()) < 3)
        {
            ++coils_count;
            auto cb = new QCheckBox("Вкл/Выкл");
            connect(cb, &QCheckBox::clicked, this, &MainBox::setCoilValue);
            w = cb;
        }
        else
        {
            ++registers_count;

            auto spin = new QSpinBox;
            spin->setRange(std::numeric_limits<qint16>::min(),
                           std::numeric_limits<qint16>::max());

            if (item->type() == Dai::Prt::itWindowState)
                spin->setValue(Dai::Prt::wCalibrated | Dai::Prt::wExecuted | Dai::Prt::wClosed);
            else
                spin->setValue(mng->needNormalize(item->type()) ? (qrand() % 100) + 240 : (qrand() % 3000) + 50);

            if (mng->registerType(item->type()) == QModbusDataUnit::HoldingRegisters)
                spin->setProperty("IsHolding", true);
            connect(spin, SIGNAL(valueChanged(int)), SLOT(setRegisterValue(int)));
            w = spin;
        }

        m_items[static_cast<QModbusDataUnit::RegisterType>(mng->registerType(item->type()))].push_back(item);
        val_idx[item] = i;

        addWidget(item, w);
    }

//    std::sort(m_items unit

//    for (int i = 0; i < max_unit; ++i) {

//    }    

    QModbusDataUnitMap reg;
    for (auto &it: m_items)
    {
        auto sort_units = [](const DevItemPtr& a, const DevItemPtr& b) -> bool
        {
            return a->unit().toInt() < b->unit().toInt();
        };
        std::sort(it.second.begin(), it.second.end(), sort_units);

        int max_unit = it.second.back()->unit().toInt();
        auto it_t = it.second.begin();
        for (int i = 0; i <= max_unit; ++i)
        {
            if (it_t == it.second.end() || (*it_t)->unit() != i)
                it_t = it.second.insert(it_t, nullptr);
            ++it_t;
        }
//            if (it.second.end() == std::find_if(it.second.begin(), it.second.end(), [i] (const DevItemPtr& item) -> bool
//            {
//                return item != nullptr && item->unit().toInt() == i;
//            }))
//            {
//                it.second.push_back(nullptr);
//            }

//        }

        QModbusDataUnit unit(it.first, 0, it.second.size()); // max_unit

        if (it.first > 2)
        {
            for (uint i = 0; i < unit.valueCount(); ++i)
            {
                auto w_it = widgets.find(it.second.at(i));
                if (w_it != widgets.cend())
                    unit.setValue(i, static_cast<QSpinBox*>(w_it->second)->value());
            }

        }

        reg.insert(it.first, unit);
    }

    modbus->setMap(reg);

    box->addStretch();
}

bool MainBox::isUsed() const { return checkBox->isChecked(); }

void MainBox::updateWidgets(QModbusDataUnit::RegisterType table, int address, int size)
{
    qDebug() << "Update data" << table << address << size;
    for (int i = 0; i < size; ++i) {
        quint16 value;
//        QString text;
        switch (table) {
        case QModbusDataUnit::Coils:
        {
            modbus->data(QModbusDataUnit::Coils, address + i, &value);

            if (auto dev_item = m_items.at(table).at(address + i))
            {
                /*if (dev_item->type() == Dai::itWindow)
                {
                    quint16 data = Dai::Prt::wCalibrated | Dai::Prt::wExecuted |
                            (value ? Dai::Prt::wOpened : Dai::Prt::wClosed);
                    modbus->setData(QModbusDataUnit::InputRegisters, address + i, data);

                    auto it = widgets.find(m_items.at(QModbusDataUnit::InputRegisters).at(address + i));
                    if (it != widgets.cend())
                        if (auto spin = qobject_cast<QSpinBox*>(it->second))
                            spin->setValue(data);
                }*/

                auto it = widgets.find(dev_item);
                if (it != widgets.cend())
                    if (auto box = qobject_cast<QCheckBox*>(it->second))
                    {
                        box->setChecked(value);
                        break;
                    }
            }
            break;
        }
        case QModbusDataUnit::HoldingRegisters:
        {
            modbus->data(QModbusDataUnit::HoldingRegisters, address + i, &value);

            auto dev_item = m_items.at(table).at(address + i);
            auto it = widgets.find(dev_item);
            if (it != widgets.cend())
            {
                if (auto spin = qobject_cast<QSpinBox*>(it->second))
                {
                    spin->setValue(value);
                    break;
                }
            }
//            registers.value(QStringLiteral("holdReg_%1").arg(address + i))->setText(text
//                .setNum(value, 16));
            break;
        }
        default: break;
        }
    }
}

void MainBox::setCoilValue(bool value)
{
    setDeviceValue(QModbusDataUnit::Coils, value);
}

void MainBox::setRegisterValue(int value)
{
    if (static_cast<QSpinBox*>(sender())->property("IsHolding").toBool())
        setDeviceValue(QModbusDataUnit::HoldingRegisters, value);
    else
        setDeviceValue(QModbusDataUnit::InputRegisters, value);
}

void MainBox::itemToggled(bool state)
{
    auto obj = sender()->children().at(1);
    auto w = qobject_cast<QSpinBox*>(obj);
    if (w)
        setDeviceValue(QModbusDataUnit::InputRegisters, state ? (quint16)w->value() : 0xFFFF, w);
}

void MainBox::setDeviceValue(QModbusDataUnit::RegisterType table, quint16 value, QWidget *w)
{
    if (!w)
        w = static_cast<QWidget*>(sender());

    auto dev_item_it = std::find_if(widgets.cbegin(), widgets.cend(), [w](auto it) {
        return it.second == w;
    });

    if (dev_item_it != widgets.cend())
    {
        auto& items = m_items.at(table);
        auto dev_item_in_items = std::find(items.cbegin(), items.cend(), dev_item_it->first);
        if (dev_item_in_items != items.cend())
        {
            std::size_t index = std::distance(items.cbegin(), dev_item_in_items);
            modbus->setData(table, index, value);
        }
    }
}

Dai::DeviceItem *MainBox::item0() { return dev->items().first(); }

