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
    'log': function(text, user_id, inform) { api.mng.log(text, 0, user_id ? user_id : 0) },
    'info': function(text, user_id, inform) { api.mng.log(text, 4, user_id ? user_id : 0) },
    'warn': function(text, user_id, inform) { api.mng.log(text, 1, user_id ? user_id : 0) },
    'critical': function(text, user_id, inform) { api.mng.log(text, 2, user_id ? user_id : 0) },
    'error': function(text, user_id, inform) { api.mng.log(text, 2, user_id ? user_id : 0) },
    'err': function(text, user_id, inform) { api.mng.log(text, 2, user_id ? user_id : 0) },
}

console.warning = console.warn;

function checkVar(state) {
    return typeof state != 'undefined'
}
