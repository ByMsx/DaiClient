#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>

#include "d_iface.h"

using namespace DBus;

PegasIface::PegasIface() :
    QObject()
{
    QDBusConnection conn = QDBusConnection::systemBus();

    // Регистрируем сервис
    if (! conn.registerService("ru.deviceaccess.Dai"))
        qCritical() << "DBus registerService fail. " + conn.lastError().message();
    else
    {
        qDebug() << "Register DBus service successfull.";

        // Регистрируем контроллер
        qDebug() << "Register DBus object is" << conn.registerObject("/", this, QDBusConnection::ExportAllContents);
    }
}

QList<GroupItem> PegasIface::test(int id)
{
    qDebug() << "GroupList PegasIface::test(int id)";
    QList<GroupItem> lts;
    GroupItem item;
    item.setName(QString("You say: %1").arg(id));
    lts.push_back(item);
    return lts;
}

void PegasIface::setFlag(int id, bool flag)
{
    emit this->flag(id, flag);
}

void PegasIface::setName(int id, QString name)
{
    emit this->name(id, name);
}

/*// Пример с хабра с отложеным ответом.
QString PegasIface::sum(int a, int b) {
    // Сообщаем DBus, что ответ прийдёт позже.
    setDelayedReply(true);

    // Это в потоке
    this->connection().send(
        this->message().createReply( a + b )
                );
    // Формальный возврат значения для компилятора. Реальный результат будет возвращён из нити.
    return 0;
}*/

// Config   --------------------------------------------------
//QString PegasIface::developerEmail() { return conf->developerEmail; }
//void PegasIface::setDeveloperEmail(QString email) { conf->developerEmail = email; }

//void setTimeout(QTimer& t, int& sec)
//{
//    if (sec <= 1) t.stop();
//    else t.setInterval( sec * 1000 );
//}

//int PegasIface::checkTimeout() { return conf->checkTimer.interval() / 1000; }
//void PegasIface::setCheckTimeout(int sec) { setTimeout(conf->checkTimer, sec); }

//int PegasIface::msgTimeout() { return conf->msgTimer.interval() / 1000; }
//void PegasIface::setMsgTimeout(int sec) { setTimeout(conf->msgTimer, sec); }

//// Database --------------------------------------------------
//QString PegasIface::dbServer() { return conf->db.server; }
//void PegasIface::dbSetServer(QString server)
//{
//    conf->db.server = server;
//    emit needReconnectDB();
//}

//QString PegasIface::dbName() { return conf->db.dbName; }
//void PegasIface::dbSetName(QString name)
//{
//    conf->db.dbName = name;
//    emit needReconnectDB();
//}

//QString PegasIface::dbUser() { return conf->db.user; }
//void PegasIface::dbSetUser(QString user)
//{
//    conf->db.user = user;
//    emit needReconnectDB();
//}

//void PegasIface::dbSetPassword(QString pwd)
//{
//    conf->db.pwd = pwd;
//    emit needReconnectDB();
//}

//// HTTP     --------------------------------------------------
//QString PegasIface::mql5Login() { return conf->http.login; }
//void PegasIface::mql5SetLogin(QString login)
//{
//    conf->http.login = login;
//    conf->http.cookies->clear();
//}

//void PegasIface::mql5SetPassword(QString pwd)
//{
//    conf->http.pwd = pwd;
//    conf->http.cookies->clear();
//}

//// EMail    --------------------------------------------------
//QString PegasIface::emailServer() { return conf->email.server; }
//void PegasIface::emailSetServer(QString server) { conf->email.server = server; }

//quint16 PegasIface::emailPort() { return conf->email.port; }
//void PegasIface::emailSetPort(int port) { conf->email.port = port; }

//int PegasIface::emailConnectionType() { return conf->email.type; }

//void PegasIface::emailSetConnectionType(int type) { conf->email.type = type; }

//QString PegasIface::emailUser() { return conf->email.user; }
//void PegasIface::emailSetUser(QString user) { conf->email.user = user; }

//void PegasIface::emailSetPassword(QString pwd) { conf->email.pwd = pwd; }

//QString PegasIface::emailPlusColor() { return conf->email.plusColor; }
//void PegasIface::emailSetPlusColor(QString color) { conf->email.plusColor = color; }

//QString PegasIface::emailMinusColor() { return conf->email.minusColor; }
//void PegasIface::emailSetMinusColor(QString color) { conf->email.minusColor = color; }

// Methods  --------------------------------------------------
void PegasIface::parse() { emit needParse(); }
//void PegasIface::saveCpnfig() { conf->save(); }
