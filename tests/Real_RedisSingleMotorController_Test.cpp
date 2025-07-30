// test_RedisSingleMotorController.cpp
#include "../exoskeleton/redis/RedisSingleMotorController.h"
#include "../exoskeleton/redis/redis_backbone.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <Qthread>
int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);

    try {
        using namespace std::chrono_literals;
          std::cout << "[DEBUG] Setting up real controller on actual motor port...\n";
          std::vector<std::string> ports = {"COM16", "COM19", "COM5","COM3","COM17"};

          std::vector<QThread*> threads;
          int n_motors = ports.size();
          int address = 0;

        sw::redis::Redis redis("tcp://127.0.0.1:6379");
        redis.del("exit");

        threads.push_back(QThread::create([]() {
                  RedisBackbone main(1s / 120);
                  main();
              }));

          for (std::string port : ports ) {
              QThread* controller_thread =QThread::create([address,port,n_motors]() {
                  RedisSingleMotorController controller(address, port,n_motors);
                  controller.loop();
              });
              threads.push_back(controller_thread);
              address++;
          }
          for (QThread* thread : threads) {
              thread->start();
          }

    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
   // return 0;
}