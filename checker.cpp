#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <QJsonArray>

#include <iostream>

#include "worker.h"
#include "checker.h"

namespace Dai {

Q_LOGGING_CATEGORY(CheckerLog, "checker")

#define MINIMAL_WRITE_INTERVAL    50

Checker::Checker(Worker *worker, int interval, const QString &pluginstr, QObject *parent) :
    QObject(parent),
    b_break(false)
{
    while (!worker->prj->ptr() && !worker->prj->wait(5));
    prj = worker->prj->ptr();

    plugin_type_mng_ = prj->plugin_type_mng_;
    loadPlugins(pluginstr.split(','), worker);

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

    for (const Plugin_Type& plugin: plugin_type_mng_->types())
        if (plugin.loader && !plugin.loader->unload())
            qWarning(CheckerLog) << "Unload fail" << plugin.loader->fileName() << plugin.loader->errorString();
}

void Checker::loadPlugins(const QStringList &allowed_plugins, Worker *worker)
{
    //    pluginLoader.emplace("modbus", nullptr);
    QString type;
    QObject *plugin;
    Plugin_Type* pl_type;
    QVector<Plugin_Type> plugins_update_vect;
    bool plugin_updated;
    Checker_Interface *checker_interface;
    QJsonObject meta_data;

    QDir pluginsDir(qApp->applicationDirPath());
    pluginsDir.cd("plugins");

    std::unique_ptr<QSettings> settings;

    auto check_is_allowed = [&allowed_plugins](const QString& fileName) -> bool
    {
        for (const QString& plugin_name: allowed_plugins)
            if (fileName.startsWith("lib" + plugin_name.trimmed()))
                return true;
        return false;
    };

    auto qJsonArray_to_qStringList = [](const QJsonArray& arr) -> QStringList
    {
        QStringList names;
        for (const QJsonValue& val: arr)
            names.push_back(val.toString());
        return names;
    };

    for (const QString& fileName: pluginsDir.entryList(QDir::Files))
    {
        if (!check_is_allowed(fileName))
            continue;

        std::shared_ptr<QPluginLoader> loader = std::make_shared<QPluginLoader>(pluginsDir.absoluteFilePath(fileName));
        if (loader->load() || loader->isLoaded())
        {
            meta_data = loader->metaData()["MetaData"].toObject();
            type = meta_data["type"].toString();

            if (!type.isEmpty() && type.length() < 128)
            {
                pl_type = plugin_type_mng_->get_type(type);
                if (pl_type->id() && pl_type->need_it && !pl_type->loader)
                {
                    qDebug(CheckerLog) << "Load plugin:" << fileName << type;

                    plugin = loader->instance();
                    checker_interface = qobject_cast<Checker_Interface *>(plugin);
                    if (checker_interface)
                    {
                        pl_type->loader = loader;
                        pl_type->checker = checker_interface;

                        if (meta_data.constFind("param") != meta_data.constEnd())
                        {
                            plugin_updated = false;
                            QJsonObject param = meta_data["param"].toObject();

                            QStringList dev_names = qJsonArray_to_qStringList(param["device"].toArray());
                            if (pl_type->param_names_device() != dev_names)
                            {
                                qCDebug(CheckerLog) << "Plugin" << pl_type->name() << "dev_names" << pl_type->param_names_device() << dev_names;
                                pl_type->set_param_names_device(dev_names);
                                plugin_updated = true;
                            }

                            QStringList dev_item_names = qJsonArray_to_qStringList(param["device_item"].toArray());
                            if (pl_type->param_names_device_item() != dev_item_names)
                            {
                                qCDebug(CheckerLog) << "Plugin" << pl_type->name() << "dev_item_names" << pl_type->param_names_device_item() << dev_item_names;
                                pl_type->set_param_names_device_item(dev_item_names);
                                if (!plugin_updated) plugin_updated = true;
                            }

                            if (plugin_updated)
                                plugins_update_vect.push_back(*pl_type);
                        }

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

    if (plugins_update_vect.size())
    {
        worker->update_plugin_param_names(plugins_update_vect);
    }
}

void Checker::breakChecking()
{
    b_break = true;

    for (const Plugin_Type& plugin: plugin_type_mng_->types())
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

        if (dev->checker_type()->loader && dev->checker_type()->checker)
            if (!dev->checker_type()->checker->check(dev))
                qCDebug(CheckerLog) << "Fail check" << dev->checker_type()->name();
    }

    if (b_break)
        return;

    if (check_timer.interval() >= MINIMAL_WRITE_INTERVAL)
        check_timer.start();

    if (write_cache_.size() && !write_timer.isActive())
        writeCache();
}

void Checker::write_data(DeviceItem *item, const QVariant &raw_data, uint32_t user_id)
{
    if (!item || !item->device())
        return;

    std::vector<Write_Cache_Item>& cache = write_cache_[item->device()->checker_type()];

    auto it = std::find(cache.begin(), cache.end(), item);
    if (it == cache.end())
    {
        cache.push_back({user_id, item, raw_data});
    }
    else if (it->raw_value_ != raw_data)
    {
        it->raw_value_ = raw_data;
    }

    if (!b_break)
        write_timer.start();
}

void Checker::writeCache()
{
    if (!check_timer.isActive() && check_timer.interval() >= MINIMAL_WRITE_INTERVAL)
        return;

    std::map<Plugin_Type*, std::vector<Write_Cache_Item>> cache(std::move(write_cache_));
    write_cache_.clear();

    while (cache.size())
    {
        write_items(cache.begin()->first, cache.begin()->second);
        cache.erase(cache.begin());
    }
}

void Checker::write_items(Plugin_Type* plugin, std::vector<Write_Cache_Item>& items)
{
    if (plugin && plugin->id() && plugin->checker)
    {
        plugin->checker->write(items);
    }
    else
    {
        for (const Write_Cache_Item& item: items)
        {
            QMetaObject::invokeMethod(item.dev_item_, "setRawValue", Qt::QueuedConnection, Q_ARG(const QVariant&, item.raw_value_), Q_ARG(bool, false), Q_ARG(uint32_t, item.user_id_));
        }
    }
}

} // namespace Dai
