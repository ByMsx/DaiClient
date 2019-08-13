#ifndef ONE_WIRE_THERM_TASK_H
#define ONE_WIRE_THERM_TASK_H

#include <QObject>
#include <QFile>
#include <map>
#include <QLoggingCategory>

namespace Dai {
class Device;
namespace OneWireTherm {

Q_DECLARE_LOGGING_CATEGORY(OneWireThermLog)

class One_Wire_Therm_Task : public QObject
{
    Q_OBJECT
public:
    explicit One_Wire_Therm_Task(QObject *parent = nullptr);

private:
    QFile file_;
    std::map<int, QString> devices_map_;

    void obtain_device_list() noexcept;
    bool get_device_file_path(int unit, QString &path_out) noexcept;
    bool try_open_file(const QString &path) noexcept;
    bool check_and_open_file(int unit) noexcept;
    bool is_error_msg = false;
public slots:
    void read_therm_data(Device *dev);
signals:
    void therm_readed();
};

} // namespace OneWireTherm
} // namespace Dai

#endif // ONE_WIRE_THERM_TASK_H
