#ifndef PARAMGROUPPROTOTYPE_H
#define PARAMGROUPPROTOTYPE_H

#include <QObject>

#include <QtScript/QScriptable>
#include <QtScript/QScriptValue>

#include "Dai/param/paramgroup.h"

namespace Dai {

class ParamGroupClass;
class ParamGroupPrototype : public QObject, public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(quint32 type READ type)
    Q_PROPERTY(int length READ length)
public:
    explicit ParamGroupPrototype(ParamGroupClass *parent = 0);

    QString title() const;

    quint32 type() const;
    QString name() const;
    int length() const;

public slots:
    QScriptValue byTypeId(quint32 param_type) const;

    QString toString() const;
    QScriptValue valueOf() const;
private:
    Param *thisParam() const;
    static Param empty_param;

    ParamGroupClass *pgClass;
};

} // namespace Dai

#endif // PARAMGROUPPROTOTYPE_H
