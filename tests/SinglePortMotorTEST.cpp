// test_RedisSingleMotorController.cpp
#include "../exoskeleton/redis/RedisSinglePortController.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <Qthread>
int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);

    try {
        std::cout << "[DEBUG] Setting up real controller on actual motor port...\n";
        std::vector<std::string> ports = {"COM3", "COM5", "COM16","COM17","COM19"};
        std::vector<QThread*> threads;
        int n_motors = ports.size();
        int address = 0;

        for (std::string port : ports ) {
            QThread* controller_thread =QThread::create([address, port,n_motors]() {
                RedisSinglePortController controller(address, port,n_motors);
                controller.loop();
            });
            threads.push_back(controller_thread);
            address++;
        }
        for (QThread* thread : threads) {
            thread->start();
        }
        sw::redis::

    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
    // return 0;
}