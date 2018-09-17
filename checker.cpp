#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFile>

#include <iostream>

#include "worker.h"
#include "checker.h"

namespace Dai {

Q_LOGGING_CATEGORY(CheckerLog, "checker")

#define MINIMAL_WRITE_INTERVAL    50

Checker::Checker(Worker *worker, int interval, const QStringList &plugins, QObject *parent) :
    QObject(parent),
    b_break(false)
{
    while (!worker->prj->ptr() && !worker->prj->wait(5));
    prj = worker->prj->ptr();

    PluginTypeMng = prj->PluginTypeMng;
    loadPlugins(plugins);




    connect(prj, &Project::controlStateChanged, this, &Checker::write_data, Qt::QueuedConnection);

    connect(prj, SIGNAL(modbusStop()), SLOT(stop()), Qt::QueuedConnection);
    connect(prj, SIGNAL(modbusStart()), SLOT(start()), Qt::QueuedConnection);
//    connect(prj, SIGNAL(modbusRead(int,uchar,int,quint16)),
//            SLOT(read2(int,uchar,int,quint16)), Qt::BlockingQueuedConnection);
//    connect(prj, SIGNAL(modbusWrite(int,uchar,int,quint16)), SLOT(write(int,uchar,int,quint16)), Qt::QueuedConnection);

    connect(&check_timer, &QTimer::timeout, this, &Checker::checkDevices);
    check_timer.setInterval(interval);
    check_timer.setSingleShot(true);

    connect(&write_timer, &QTimer::timeout, this, &Checker::writeCache);
    write_timer.setInterval(MINIMAL_WRITE_INTERVAL);
    write_timer.setSingleShot(true);
    // --------------------------------------------------------------------------------



    checkDevices(); // Первый опрос контроллеров
    QMetaObject::invokeMethod(prj, "afterAllInitialization", Qt::QueuedConnection);
}

Checker::~Checker()
{
    stop();

    for (const PluginType& plugin: PluginTypeMng->types())
        if (plugin.loader && !plugin.loader->unload())
            qWarning(CheckerLog) << "Unload fail" << plugin.loader->fileName() << plugin.loader->errorString();
}

void Checker::loadPlugins(const QStringList &allowed_plugins)
{
    //    pluginLoader.emplace("modbus", nullptr);
    QString type;
    QObject *plugin;
    PluginType* pl_type;
    CheckerInterface *checkerInterface;

    QDir pluginsDir(qApp->applicationDirPath());
    pluginsDir.cd("plugins");

    std::unique_ptr<QSettings> settings;

    bool finded;
    for (const QString& fileName: pluginsDir.entryList(QDir::Files))
    {
        finded = false;
        for (const QString& plugin_name: allowed_plugins)
            if (fileName.startsWith("lib" + plugin_name)) {
                finded = true;
                break;
            }
        if (!finded)
            continue;

        std::shared_ptr<QPluginLoader> loader = std::make_shared<QPluginLoader>(pluginsDir.absoluteFilePath(fileName));
        if (loader->load() || loader->isLoaded())
        {
            type = loader->metaData()["MetaData"].toObject()["type"].toString();

            if (!type.isEmpty() && type.length() < 128)
            {
                pl_type = PluginTypeMng->getType(type);
                if (pl_type->id() && pl_type->need_it && !pl_type->loader)
                {
                    qDebug(CheckerLog) << "Load plugin:" << fileName << type;

                    plugin = loader->instance();
                    checkerInterface = qobject_cast<CheckerInterface *>(plugin);
                    if (checkerInterface)
                    {
                        pl_type->loader = loader;
                        pl_type->checker = checkerInterface;

                        if (!settings)
                            settings = Worker::settings();
                        pl_type->checker->configure(settings.get(), prj);
                        continue;
                    }
                    else
                        qWarning(CheckerLog) << "Bad plugin" << plugin << loader->errorString();
                }
            }
            else
                qWarning(CheckerLog) << "Bad type in plugin" << fileName << type;

            loader->unload();
        }
        else
            qWarning(CheckerLog) << "Fail to load plugin" << fileName << loader->errorString();
    }
}

void Checker::breakChecking()
{
    b_break = true;

    for (const PluginType& plugin: PluginTypeMng->types())
        if (plugin.loader && plugin.checker)
            plugin.checker->stop();
}

void Checker::stop()
{
    qCDebug(CheckerLog) << "Check stoped";

    if (check_timer.isActive())
        check_timer.stop();

    breakChecking();
}

void Checker::start()
{
    qCDebug(CheckerLog) << "Start check";
    checkDevices();
}

void Checker::checkDevices()
{
    b_break = false;

    for (Device* dev: prj->devices())
    {
        if (b_break) break;

        if (dev->items().size() == 0) continue;

        if (dev->checkerType()->loader && dev->checkerType()->checker)
            if (!dev->checkerType()->checker->check(dev))
                qCDebug(CheckerLog) << "Fail check" << dev->checkerType()->name();
    }

    if (b_break)
        return;

    if (check_timer.interval() >= MINIMAL_WRITE_INTERVAL)
        check_timer.start();

    if (m_writeCache.size() && !write_timer.isActive())
        writeCache();
}

void Checker::write_data(DeviceItem *item, const QVariant &raw_data)
{
    auto it = m_writeCache.find(item);
    if (it == m_writeCache.end())
        m_writeCache.emplace(item, raw_data);
    else if (it->second != raw_data)
        it->second = raw_data;

    if (!b_break)
        write_timer.start();
}

void Checker::writeCache()
{
    if (!check_timer.isActive() && check_timer.interval() >= MINIMAL_WRITE_INTERVAL)
        return;

    while (m_writeCache.size()) {
        auto iterator = m_writeCache.begin();
        auto [dev_item, raw_data] = *iterator;
        m_writeCache.erase(iterator);

        writeItem(dev_item, raw_data);
    }
}

void Checker::writeItem(DeviceItem *item, const QVariant &raw_data)
{
    PluginType* chk_type = item->device()->checkerType();
    if (chk_type && chk_type->id() && chk_type->checker)
        chk_type->checker->write(item, raw_data);
    else
    {
        QMetaObject::invokeMethod(item, "setRawValue", Qt::QueuedConnection, Q_ARG(QVariant, raw_data));

        if (!chk_type || (chk_type->id() && !chk_type->checker))
            qCWarning(CheckerLog) << "Checker not initialized" << item->toString() << (chk_type ? ("%1 " + chk_type->name()).arg(chk_type->id()) : QString());
    }
}

} // namespace Dai
