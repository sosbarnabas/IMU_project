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
        std::vector<std::string> ports = {"COM3", "COM5", "COM16","COM17","COM19"};
        std::vector<QThread*> threads;
        int n_motors = ports.size();
        int address = 0;

        for (std::string port : ports ) {
            QThread* controller_thread =QThread::create([address, port,n_motors]() {
                RedisSingleMotorController controller(address, port,n_motors);
                controller.loop();
            });
            threads.push_back(controller_thread);
            address++;
        }
        for (QThread* thread : threads) {
            thread->start();
        }
        sw::redis::Redis redis("tcp://127.0.0.1:6379");
        redis.lpush("started:0","start");
        redis.lpush("started:1","start");
        redis.lpush("started:2","start");
        redis.lpush("started:3","start");
        redis.lpush("started:4","start");

    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
   // return 0;
}