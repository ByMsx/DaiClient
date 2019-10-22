#include <QtScript/QScriptClassPropertyIterator>
#include <QtScript/QScriptContext>
#include <QtScript/QScriptEngine>

#include <QDateTime>
#include <QDebug>

#include "paramgroupclass.h"
#include "paramgroupprototype.h"

Q_DECLARE_METATYPE(Dai::ParamGroupClass*)

namespace Dai {

class ParamGroupClassPropertyIterator : public QScriptClassPropertyIterator
{
public:
    ParamGroupClassPropertyIterator(const QScriptValue &object);
    ~ParamGroupClassPropertyIterator();

    bool hasNext() const override;
    void next() override;

    bool hasPrevious() const override;
    void previous() override;

    void toFront() override;
    void toBack() override;

    QScriptString name() const override;
    uint id() const override;

private:
    uint32_t m_index;
    int m_last;
};

ParamGroupClass::ParamGroupClass(QScriptEngine *engine) :
    QObject(engine), QScriptClass(engine)
{
    qScriptRegisterMetaType<Dai::Param*>(engine, toScriptValue, fromScriptValue);

    proto = engine->newQObject(new ParamGroupPrototype(this),
                               QScriptEngine::QtOwnership,
                               QScriptEngine::SkipMethodsInEnumeration
                               | QScriptEngine::ExcludeSuperClassMethods
                               | QScriptEngine::ExcludeSuperClassProperties);
    QScriptValue global = engine->globalObject();
    proto.setPrototype(global.property("Object").property("prototype"));

    //engine->setDefaultPrototype(qMetaTypeId<Param*>(), proto);

    f_ctor = engine->newFunction(construct, proto);
    f_ctor.setData(engine->toScriptValue(this));
}

QScriptValue ParamGroupClass::newInstance(Param *param)
{
    return ParamGroupClass::newInstance(this, param);
}

QScriptValue ParamGroupClass::newInstance(ParamGroupClass *pgClass, Param *param)
{
    QScriptValue data = pgClass->engine()->newVariant(QVariant::fromValue(param));
    return pgClass->engine()->newObject(pgClass, data);
}

QScriptValue ParamGroupClass::getValue(Param *param)
{
    return ParamGroupClass::getValue(this, param);
}

/*static*/ QScriptValue ParamGroupClass::getValue(ParamGroupClass *pgClass, Param *param)
{
    switch (param->type()->type()) {
    case Param_Type::TimeType:
    case Param_Type::IntType:       return param->value().toInt();
    case Param_Type::BoolType:      return param->value().toInt() ? true : false;
    case Param_Type::FloatType:     return param->value().toReal();
    case Param_Type::BytesType:     return QString::fromLocal8Bit(param->value().toByteArray().toHex());
    case Param_Type::RangeType:
        if (pgClass)
            return ParamGroupClass::newInstance(pgClass, const_cast<Param*>(param));
        else
            return param->value().toString();
    case Param_Type::ComboType:
    case Param_Type::StringType:
    default:
        return param->value().toString();
    }
}

QScriptClass::QueryFlags ParamGroupClass::queryProperty(const QScriptValue &object, const QScriptString &name,
                                                        QScriptClass::QueryFlags flags, uint *id)
{
    Param* param = nullptr;
    fromScriptValue(object, param);
    if (!param || !param->count())
        return 0;

    bool isArrayIndex;
    quint32 pos = name.toArrayIndex(&isArrayIndex);
    if (!isArrayIndex)
        return param->has(name.toString()) ? flags : static_cast<QScriptClass::QueryFlags>(0);

    *id = pos;
    if ((flags & HandlesReadAccess) && (pos >= param->count()))
        flags &= ~HandlesReadAccess;
    return flags;

//    qWarning() << "queryProperty" << name << param << object.data().toVariant().value<Param*>() << flags << (flags == 0);
}

QScriptValue ParamGroupClass::property(const QScriptValue &object, const QScriptString &name, uint id)
{
    Param* param = nullptr;
    fromScriptValue(object, param);
    if (!param || !param->count())
        return QScriptValue();

    bool isArrayIndex;
    name.toArrayIndex(&isArrayIndex);

    Param* child_param = isArrayIndex ? param->get(id) : param->get(name.toString());
    if (!child_param)
        return QScriptValue();
    return toScriptValue(engine(), child_param);

    Param* elem = param->get(name.toString());
    if (!elem)
        return QScriptValue();

    return getValue(elem);
}

void ParamGroupClass::setProperty(QScriptValue &object, const QScriptString &name, uint id, const QScriptValue &value)
{
    Param* param = nullptr;
    fromScriptValue(object, param);
    if (!param)
        return;

    qInfo() << "setProperty" << name << param->type()->title() << id << value.toVariant();

    bool isArrayIndex;
    name.toArrayIndex(&isArrayIndex);

    Param* child_param = isArrayIndex ? param->get(id) : param->get(name.toString());
    if (child_param)
        child_param->set_value(value.toVariant());

//    if (child_param->type().type == Param_Type::TimeType)
//    {
//        if (value.isDate())
//            child_param->setValue(value.toDateTime().toMSecsSinceEpoch());
//    }
//    else
//        child_param->setValue(value.toVariant());
}

QScriptValue::PropertyFlags ParamGroupClass::propertyFlags(const QScriptValue &object, const QScriptString &name, uint /*id*/)
{
    Param* param = nullptr;
    fromScriptValue(object, param);
    if (!param)
        return 0;

    QScriptValue::PropertyFlags flags = QScriptValue::Undeletable;

    if (!param->get(name.toString()))
        flags |= QScriptValue::SkipInEnumeration;
    return flags;

    qWarning() << "propertyFlags" << name;
}

QScriptClassPropertyIterator *ParamGroupClass::newIterator(const QScriptValue &object)
{
    return new ParamGroupClassPropertyIterator(object);
}

QString ParamGroupClass::name() const { return QLatin1String("Params"); }
QScriptValue ParamGroupClass::prototype() const { return proto; }
QScriptValue ParamGroupClass::constructor() { return f_ctor; }

// ----- STATIC -----
QScriptValue ParamGroupClass::construct(QScriptContext *ctx, QScriptEngine *)
{
    ParamGroupClass *cls = qscriptvalue_cast<ParamGroupClass*>(ctx->callee().data());
    if (cls) {
        QScriptValue arg = ctx->argument(0);
        if (arg.instanceOf(ctx->callee()))
            return cls->newInstance(qscriptvalue_cast<Param*>(arg));
    }
    return QScriptValue();
}

QScriptValue ParamGroupClass::toScriptValue(QScriptEngine *eng, Param * const & param)
{
    QScriptValue ctor = eng->globalObject().property("Params");
    ParamGroupClass *cls = qscriptvalue_cast<ParamGroupClass*>(ctor.data());
    if (!cls)
        return eng->newVariant(QVariant::fromValue(param));
    return cls->newInstance(param);
}

void ParamGroupClass::fromScriptValue(const QScriptValue &obj, Param* & param)
{
    param = obj.data().toVariant().value<Param*>();
}
// ----- /STATIC -----

// ----> ParamGroupClassPropertyIterator
ParamGroupClassPropertyIterator::ParamGroupClassPropertyIterator(const QScriptValue &object)
    : QScriptClassPropertyIterator(object)
{
    toFront();
}

ParamGroupClassPropertyIterator::~ParamGroupClassPropertyIterator()
{
}

//! [8]
bool ParamGroupClassPropertyIterator::hasNext() const
{
    Param* param = nullptr;
    ParamGroupClass::fromScriptValue(object(), param);
    return param && m_index < param->count();
}

void ParamGroupClassPropertyIterator::next()
{
    m_last = m_index;
    ++m_index;
}

bool ParamGroupClassPropertyIterator::hasPrevious() const
{
    return (m_index > 0);
}

void ParamGroupClassPropertyIterator::previous()
{
    --m_index;
    m_last = m_index;
}

void ParamGroupClassPropertyIterator::toFront()
{
    m_index = 0;
    m_last = -1;
}

void ParamGroupClassPropertyIterator::toBack()
{
    Param* param = nullptr;
    ParamGroupClass::fromScriptValue(object(), param);
    m_index = param ? param->count() : 0;
    m_last = -1;
}

QScriptString ParamGroupClassPropertyIterator::name() const
{
    Param* param = nullptr;
    ParamGroupClass::fromScriptValue(object(), param);
    if (param) {
        Param* p = param->get(m_last);
        if (p) {
            return object().engine()->toStringHandle(p->type()->name());
        }
    }
    return QScriptString();
}

uint ParamGroupClassPropertyIterator::id() const
{
    return m_last;
}

} // namespace Dai
