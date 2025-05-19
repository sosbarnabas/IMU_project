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

       std::thread controller_thread([address, port]() {
          RedisSingleMotorController controller(address, port,2);
          controller.loop();
       });



        controller_thread.join();
       // std::cout << "[DEBUG] Starting controller loop thread...\n";
       // std::thread controller_thread([&controller]() {
       //     controller.loop();
       // });
//
       //std::this_thread::sleep_for(std::chrono::seconds(1));
       //sw::redis::Redis redis("tcp://127.0.0.1:6379");
       // std::cout << "[DEBUG] Signaling start...\n";
       // redis.lpush("started:" + std::to_string(address), "start");
//
       // std::cout << "[DEBUG] Test completed successfully.\n";
    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
   // return 0;
}