
load(moc)

QDBUS_FLAG =
equals(DBUS_TYPE, "adaptor") {
    QDBUS_FLAG = -a
}
equals(DBUS_TYPE, "interface") {
    QDBUS_FLAG = -p
}

isEmpty(QDBUS_FLAG) {
    error("Please set DBUS_TYPE adaptor or interface, now is:" $$DBUS_TYPE $$QDBUS_FLAG)
}

DBUS_INCLUDE =
for (inc_it, DBUS_INCLUDES) DBUS_INCLUDE += -i $$inc_it

mydbus.name = My DBus
mydbus.CONFIG += target_predeps
mydbus.input = DBUS_FILES
mydbus.output = $${OUT_PWD}/${QMAKE_FILE_IN_BASE}_$${DBUS_TYPE}.h
mydbus.commands = qdbusxml2cpp ${QMAKE_FILE_IN} $$DBUS_INCLUDE $$QDBUS_FLAG $${OUT_PWD}/${QMAKE_FILE_IN_BASE}_$${DBUS_TYPE}
mydbus.variable_out = MYDBUS_HEADERS

mydbus_impl.name = My DBus Impl
mydbus_impl.CONFIG += target_predeps
mydbus_impl.input = DBUS_FILES
mydbus_impl.output = $${OUT_PWD}/${QMAKE_FILE_IN_BASE}_$${DBUS_TYPE}.cpp
mydbus_impl.commands = $$escape_expand(\n)
mydbus_impl.variable_out = SOURCES

mydbus_moc.CONFIG += target_predeps
mydbus_moc.commands = $$moc_header.commands
mydbus_moc.output = $$moc_header.output
mydbus_moc.input = MYDBUS_HEADERS
mydbus_moc.variable_out = GENERATED_SOURCES
mydbus_moc.name = MY_$$moc_header.name
mydbus_moc.depends = ${QMAKE_FILE_IN_BASE}.cpp

QMAKE_EXTRA_COMPILERS += mydbus mydbus_impl mydbus_moc
