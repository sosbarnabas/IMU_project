//
// Created by robotlab on 27/05/2025.
//
#include <iostream>
#include "../exoskeleton/motor/motor.h"
#include <QCoreApplication>
#include <iostream>
#include <string>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <stdexcept>
#include <thread>
#include <chrono>

#include "../exoskeleton/motor/motor.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
        if (!port.serialNumber().isEmpty() && port.serialNumber().startsWith("CSTNY")) {
            std::cout << port.serialNumber().toStdString() << ":\t" << port.portName().toStdString() << '\n';
        }
    }

    const auto ser = exoskeleton::motor::open_serial_port(exoskeleton::motor::find_port_name_by_serial_num("CSTNY004"));
    const size_t n {1000};
    std::vector<decltype(std::chrono::milliseconds{1}.count())> times(n);
    for (auto i = 0 ; i < n; i++) {
        auto t0 = std::chrono::steady_clock::now();
        auto data = exoskeleton::motor::read_data(*ser,3);
        auto t1 = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0);
        times[i] = dur.count();
        // std::cerr << dur.count() << "ms " << data << std::endl;
    }
    std::cout << static_cast<long double>(std::reduce(times.begin(), times.end())) / n << std::endl;
    return app.exec();
}
