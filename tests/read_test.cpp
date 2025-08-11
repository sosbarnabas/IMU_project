//
// Created by robotlab on 27/05/2025.
//
#include <iostream>
#include "E:/Samu/repo/exo-cpp-orbsa/exoskeleton/motor/motor.h"
#include <QCoreApplication>
#include <iostream>
#include <string>
#include <QSerialPort>
#include <stdexcept>
#include <thread>
#include <chrono>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
        if (!port.serialNumber().isEmpty() && port.serialNumber().startsWith("CSTNY")) {
            std::cout << port.serialNumber().toStdString() << ":\t" << port.portName().toStdString() << '\n';
        }
    }

    QSerialPort* ser = exoskeleton::motor::open_serial();
    for (int i = 0 ; i < 1000; i++) {
        auto t0 = std::chrono::steady_clock::now();
        auto data = exoskeleton::motor::read_data(ser,3);
        auto t1 = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0);
        std::cerr << dur.count() << "ms " << data << std::endl;
    }
    ser->deleteLater();
    return app.exec();
}