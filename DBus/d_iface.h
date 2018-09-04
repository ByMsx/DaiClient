#ifndef DBUS_MQL5IFACE_H
#define DBUS_MQL5IFACE_H

#include <QObject>
#include <QDBusContext>

#include "message.h"

namespace DBus {

class PegasIface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "ru.deviceaccess.Dai.iface")
public:
    PegasIface();

    Q_INVOKABLE QList<GroupItem> test(int id);
    Q_INVOKABLE void setFlag(int id, bool flag);
    Q_INVOKABLE void setName(int id, QString name);

//    Q_INVOKABLE QString sum(int a, int b);

    // Config     --------------------------------------------------
//    Q_INVOKABLE QString developerEmail();
//    Q_INVOKABLE void setDeveloperEmail(QString email);

//    Q_INVOKABLE int checkTimeout();
//    Q_INVOKABLE void setCheckTimeout(int sec);

//    Q_INVOKABLE int msgTimeout();
//    Q_INVOKABLE void setMsgTimeout(int sec);

//    // Database --------------------------------------------------
//    Q_INVOKABLE QString dbServer();
//    Q_INVOKABLE void dbSetServer(QString server);

//    Q_INVOKABLE QString dbName();
//    Q_INVOKABLE void dbSetName(QString name);

//    Q_INVOKABLE QString dbUser();
//    Q_INVOKABLE void dbSetUser(QString user);

//    Q_INVOKABLE void dbSetPassword(QString pwd);

//    // HTTP     --------------------------------------------------
//    Q_INVOKABLE QString mql5Login();
//    Q_INVOKABLE void mql5SetLogin(QString login);
//    Q_INVOKABLE void mql5SetPassword(QString pwd);

//    // EMail    --------------------------------------------------
//    Q_INVOKABLE QString emailServer();
//    Q_INVOKABLE void emailSetServer(QString server);

//    Q_INVOKABLE quint16 emailPort();
//    Q_INVOKABLE void emailSetPort(int port);

//    Q_INVOKABLE int emailConnectionType();
//    Q_INVOKABLE void emailSetConnectionType(int type);

//    Q_INVOKABLE QString emailUser();
//    Q_INVOKABLE void emailSetUser(QString user);

//    Q_INVOKABLE void emailSetPassword(QString pwd);

//    Q_INVOKABLE QString emailPlusColor();
//    Q_INVOKABLE void emailSetPlusColor(QString color);
//    Q_INVOKABLE QString emailMinusColor();
//    Q_INVOKABLE void emailSetMinusColor(QString color);
    // Methods
//    Q_INVOKABLE void saveCpnfig();
    Q_INVOKABLE void parse();
private:
//    Config* conf;

signals:
    void needReconnectDB();
    void needParse();

    void flag(int id, bool flag);
    void name(int id, QString name);
public slots:
};

} // namespace DBus

#endif // DBUS_MQL5IFACE_H
