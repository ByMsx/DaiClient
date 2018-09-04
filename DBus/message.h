#ifndef MESSAGE_HPP
#define MESSAGE_HPP
 
#include <QtDBus>

#include <map>

typedef std::map<int, QString> SectionMap;
QDBusArgument &operator<<(QDBusArgument &argument, const SectionMap &scts);
const QDBusArgument &operator>>(const QDBusArgument &argument, SectionMap &scts);

Q_DECLARE_METATYPE(SectionMap)
//Q_DECLARE_METATYPE(std::string)


class GroupItem
{
public:
    GroupItem() = default;

    friend QDBusArgument &operator<<(QDBusArgument &argument, const GroupItem &lts);
    friend const QDBusArgument &operator>>(const QDBusArgument &argument, GroupItem &lts);
 
    QString name() const;
    void setName(const QString &name);

    int status() const;
    void setStatus(int status);

    uchar type() const;
    void setType(const uchar &type);

    QVariant value() const;
    void setValue(const QVariant &value);

private:
    QString m_name;
    int m_status;
    uchar m_type;
    QVariant m_value;
};
Q_DECLARE_METATYPE(GroupItem)

typedef std::vector<GroupItem> GroupList;
Q_DECLARE_METATYPE(GroupList)
QDBusArgument &operator<<(QDBusArgument &argument, const GroupList &lts);
const QDBusArgument &operator>>(const QDBusArgument &argument, GroupList &lts);
 
#endif // MESSAGE_HPP
