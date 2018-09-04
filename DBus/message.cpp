#include "message.h"


QDBusArgument &operator<<(QDBusArgument &argument, const GroupItem &group)
{
    argument.beginStructure();
    argument << group.m_name;
    argument << group.m_status;
    argument << group.m_type;
    argument << group.m_value.isValid() << QDBusVariant(group.m_value.isValid() ? group.m_value : 0);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, GroupItem &group)
{
    argument.beginStructure();
    argument >> group.m_name;
    argument >> group.m_status;
    argument >> group.m_type;

    bool isValid;
    QDBusVariant data;
    argument >> isValid >> data;
    group.m_value = isValid ? data.variant() : QVariant();

    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const GroupList &groups)
{
    argument.beginArray( qMetaTypeId<GroupItem>() );

    for (auto& group: groups)
        argument << group;

    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, GroupList &groups)
{
    groups.clear();

    argument.beginArray();

    while ( !argument.atEnd() )
    {
        GroupItem item;
        argument >> item;
        groups.push_back( item );
    }

    argument.endArray();
    return argument;
}
 
//QDBusArgument &operator<<(QDBusArgument &argument, const Message& message)
//{
//    argument.beginStructure();
//    argument << message.m_user;
//    argument << message.m_text;
//    argument.endStructure();
 
//    return argument;
//}
 
//const QDBusArgument &operator>>(const QDBusArgument &argument, Message &message)
//{
//    argument.beginStructure();
//    argument >> message.m_user;
//    argument >> message.m_text;
//    argument.endStructure();
 
//    return argument;
//}


QDBusArgument &operator<<(QDBusArgument &argument, const SectionMap &scts)
{
    argument.beginMap( QVariant::Int, QVariant::String/*qMetaTypeId<QString>()*/ );

    for (auto it: scts)
    {
        argument.beginMapEntry();
        argument << it.first << it.second;
        argument.endMapEntry();
    }

    argument.endMap();
    return argument;
}


const QDBusArgument &operator>>(const QDBusArgument &argument, SectionMap &scts)
{
    argument.beginMap();
    scts.clear();

    while ( !argument.atEnd() )
    {
        int key;
        QString value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        scts[ key ] = value;
    }

    argument.endMap();
    return argument;
}

QString GroupItem::name() const
{
    return m_name;
}

void GroupItem::setName(const QString &name)
{
    m_name = name;
}

int GroupItem::status() const
{
    return m_status;
}

void GroupItem::setStatus(int status)
{
    m_status = status;
}

uchar GroupItem::type() const
{
    return m_type;
}

void GroupItem::setType(const uchar &type)
{
    m_type = type;
}

QVariant GroupItem::value() const
{
    return m_value;
}

void GroupItem::setValue(const QVariant &value)
{
    m_value = value;
}
