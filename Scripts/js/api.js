var api = {
    version: 200,

    actDevice: function(group, type, newState, user_id) {
        group.writeToControl(type, newState, api.type.mode.automatic, user_id)
    },
    findItem: function(items, func) {
        for (var item_idx in items)
        {
            var item = items[item_idx];
            if (func(item))
                return item;
        }
        return null;
    },
    status: {},
    type: {
        item: {}, group: {}, mode: {}, param: {}
    },
    checker: [],

    handlers: {
        changed: {
            mode: undefined,
            item: undefined,
            sensor: undefined,
            control: undefined,
            day_part: undefined,
        },
        database: { initialized: undefined },
        section: { initialized: undefined },
        group: { initialized: {}, changed: {} }, // fill group names
        control_change_check: undefined,
        normalize: undefined,
        check_value: undefined,
        group_status: undefined,
        initialized: undefined,
    },
};

api.extend = function(Child, Parent)
{
    var F = function() {};
    F.prototype = Parent.prototype;
    Child.prototype = new F();
    Child.prototype.constructor = Child;
    Child.superclass = Parent.prototype;
};

api.mixin = function(dst, src)
{
    var tobj = {};
    for(var x in src)
        if((typeof tobj[x] == "undefined") || (tobj[x] != src[x]))
            dst[x] = src[x];
};

api.get_type_name = function(type_id, types)
{
    for (var i in types)
        if (types[i] === type_id)
            return i;
    return '[unknown_type]';
};

api.connect_if_exist = function(signal, obj, func_name)
{
    if (typeof obj[func_name] === 'function')
        signal.connect(obj, obj[func_name]);
};

api.init_as_group_manager = function(obj, group)
{
    obj.group = group;
    obj.param = group.param;
    obj.item = {};

    var items = group.items;
    for (var i in items)
    {
        var item = items[i];
        var type_name = api.get_type_name(item.type, api.type.item);
        obj.item[type_name] = item;

        api.connect_if_exist(item.valueChanged, obj, 'on_' + type_name); // args: user_id
    }

    api.connect_if_exist(group.modeChanged , obj, 'on_mode_changed');  // args: user_id, mode_id
    api.connect_if_exist(group.itemChanged , obj, 'on_item_changed');  // args: item, user_id
    api.connect_if_exist(group.paramChanged, obj, 'on_param_changed'); // args: param, user_id
};

var modbus = {

    DiscreteInputs: 1, DI: 1,
    Coils: 2, C: 2,
    InputRegisters: 3, IR: 3,
    HoldingRegisters: 4, HR:4,

    read: function(server, regType, address, count) {
        regType = regType !== undefined ? regType : 3;
        address = address !== undefined ? address : 0;
        count = count !== undefined ? count : 1;

        return api.mng.modbusRead(server, regType, address, count);
    },

    write: function(server, regType, unit, value) {
        api.mng.modbusWrite(server, regType, unit, value);
    },

    stop: function() { api.mng.modbusStop(); },
    start: function() { api.mng.modbusStart(); },
}

var console = {
    'log': function(text, user_id, inform) { api.mng.log(text, 0, user_id, inform) },
    'info': function(text, user_id, inform) { api.mng.log(text, 4, user_id, inform) },
    'warn': function(text, user_id, inform) { api.mng.log(text, 1, user_id, inform) },
    'critical': function(text, user_id, inform) { api.mng.log(text, 2, user_id, inform) },
    'error': function(text, user_id, inform) { api.mng.log(text, 2, user_id, inform) },
    'err': function(text, user_id, inform) { api.mng.log(text, 2, user_id, inform) },
}

console.warning = console.warn;

function checkVar(state) {
    return typeof state != 'undefined'
}
