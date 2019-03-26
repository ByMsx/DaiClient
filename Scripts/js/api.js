var api = {
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
    checker: []
}

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
    'log': function(text) { api.mng.log(text, 0) },
    'info': function(text) { api.mng.log(text, 4) },
    'warn': function(text) { api.mng.log(text, 1) },
    'critical': function(text) { api.mng.log(text, 2) },
    'error': function(text) { api.mng.log(text, 2) },
    'err': function(text) { api.mng.log(text, 2) },
}

console.warning = console.warn;

function checkVar(state) {
    return typeof state != 'undefined'
}
