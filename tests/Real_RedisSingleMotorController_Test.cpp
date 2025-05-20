// test_RedisSingleMotorController.cpp
#include "../exoskeleton/redis/RedisSingleMotorController.h"
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

        int address = 0;
        std::string port = "COM3";

        int address2 = 1;
        std::string port2 = "COM17";

       QThread* controller_thread =QThread::create([address, port]() {
          RedisSingleMotorController controller(address, port,2);
          controller.loop();
       });


       QThread* controller_thread2 = QThread::create([address2, port2]() {
    RedisSingleMotorController controller(address2, port2,2);
    controller.loop();
       });
        controller_thread->start();
        controller_thread2->start();
    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
   // return 0;
}