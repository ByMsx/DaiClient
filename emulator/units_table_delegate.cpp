#include "units_table_delegate.h"

#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>

#include <Dai/deviceitem.h>
#include "Dai/typemanager/typemanager.h"
#include <QModbusDataUnit>
#include <units_table_model.h>

Units_Table_Delegate::Units_Table_Delegate(QObject *parent) : QStyledItemDelegate(parent)
{

}


QWidget *Units_Table_Delegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int reg_type = index.data(Units_Table_Model::UNIT_TYPE_ROLE).toInt();
    switch (reg_type)
    {
        case QModbusDataUnit::DiscreteInputs       :
        case QModbusDataUnit::Coils                :
        {
            QCheckBox* check_box = new QCheckBox(parent);
            check_box->setText("On/Off");
            return check_box;
        }
        case QModbusDataUnit::InputRegisters       :
        case QModbusDataUnit::HoldingRegisters     :
        {
            QSpinBox *spin_box = new QSpinBox(parent);
            spin_box->setRange(std::numeric_limits<qint16>::min(),
                           std::numeric_limits<qint16>::max());
            return spin_box;
        }
        default:
            return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void Units_Table_Delegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int reg_type = index.data(Units_Table_Model::UNIT_TYPE_ROLE).toInt();
    switch (reg_type)
    {
        case QModbusDataUnit::DiscreteInputs       :
        case QModbusDataUnit::Coils                :
        {
            bool is_check = index.model()->data(index, Qt::CheckStateRole).toBool();
            QCheckBox *check_box = static_cast<QCheckBox*>(editor);
            check_box->setChecked(is_check);
            return;
        }
        case QModbusDataUnit::InputRegisters       :
        case QModbusDataUnit::HoldingRegisters     :
        {
            int value = index.model()->data(index, Qt::EditRole).toInt();
            QSpinBox *spin_box = static_cast<QSpinBox*>(editor);
            spin_box->setValue(value);
            return;
        }
        default:
            return;
    }
}

void Units_Table_Delegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int reg_type = index.data(Units_Table_Model::UNIT_TYPE_ROLE).toInt();

    int role = Qt::DisplayRole;
    QVariant value;
    switch (reg_type)
    {
        case QModbusDataUnit::DiscreteInputs       :
        case QModbusDataUnit::Coils                :
        {
            role = Qt::EditRole;
            QCheckBox *check_box = static_cast<QCheckBox*>(editor);
            value = check_box->isChecked();
            break;
        }
        case QModbusDataUnit::InputRegisters       :
        case QModbusDataUnit::HoldingRegisters     :
        {
            role = Qt::EditRole;
            QSpinBox *spin_box = static_cast<QSpinBox*>(editor);
            spin_box->interpretText();
            value = spin_box->value();
            break;
        }
        default:
            return;
    }

    model->setData(index, value, role);
}

void Units_Table_Delegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    editor->setGeometry(option.rect);
}
