#include <iostream>
#include "E:/Samu/repo/exo-cpp-orbsa/exoskeleton/motor/motor.h"
#include <QCoreApplication>
#include <iostream>
#include <string>
#include <QSerialPort>
#include <stdexcept>
#include <thread>
#include <chrono>
using namespace exoskeleton::motor;

/*
import exoskeleton.motor as m
ser = m.open_serial()
m.motor.log_command = True
m.motor_disable(ser)
ser.close()
*/

auto call_command(QSerialPort* ser, auto func) -> exoskeleton::motor::SingleMotorData {
    auto const r1 = read_data(ser);
    std::cerr << "before call_command " << r1 << std::endl;
    func();
    //std::this_thread::sleep_for(std::chrono::milliseconds{100});
    auto const r2 = read_data(ser);
    std::cerr << "after call_command " << r2 << std::endl;
    if (r1.cmd_cntr == r2.cmd_cntr) {
        throw std::runtime_error{"Baj van"};
    }
    return r2;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QSerialPort* ser = exoskeleton::motor::open_serial();
    motor_disable(ser);

    for (int i = 0 ; i < 10; i++) {
        std::cerr << read_data(ser) << std::endl;
    }
    motor_enable(ser);
    for (int i = 0 ; i < 10; i++) {
        std::cerr << read_data(ser) << std::endl;
    }
    auto const fn1 = std::vector<int>(360, 0);
    auto const fn2 = std::vector<int>(360, 30);
    std::vector<int> fn3;
    for (int i = 0; i < 360; i++) {
        if (i < 100) {
            fn3.push_back(110);
        }
        else if (i >= 100 && i < 180) {
            fn3.push_back((110-(i-100)));
        }
        else {
            fn3.push_back(30);
        }
    }
    call_command(ser, [ser, &fn3]{ motor_set_slot_function(ser,0,fn3); });
    call_command(ser, [ser, &fn3]{ motor_set_slot_function(ser,7,fn3); });
    call_command(ser, [ser]{ motor_select_slot(ser,7); });
    for (int i = 0 ; i < 500; i++) {
        std::cerr << read_data(ser) << std::endl;
    }
    std::cout << call_command(ser, [ser]{ motor_select_slot(ser,0); }) << std::endl;
    call_command(ser, [ser]{ motor_disable(ser); });
    std::cerr << "---------DONE---------" << std::endl;
    return app.exec();
}
