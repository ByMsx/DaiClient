TEMPLATE = subdirs
SUBDIRS += ModbusPlugin RandomPlugin OneWireThermPlugin

linux-rasp-pi2-g++|linux-rasp-pi3-g++|linux-opi-g++ {
    SUBDIRS += WiringPiPlugin
}
