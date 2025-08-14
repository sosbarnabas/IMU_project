// test_RedisSingleMotorController.cpp
#include "../exoskeleton/multiport/RedisSingleMotorController.h"
#include "../exoskeleton/core/redis_backbone.h"
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
        std::vector<std::string> serial_numbers = {"CSTNY004", "CSTNY005", "CSTNY006","CSTNY007","CSTNY003"};

        std::vector<QThread*> threads;
        int n_motors = serial_numbers.size();
        int address = 0;

        sw::redis::Redis redis("tcp://127.0.0.1:6379");
        redis.del("exit");

        threads.push_back(QThread::create([]() {
            exoskeleton::core::RedisBackbone main(1s / 120);
            main();
        }));

        for (const std::string& sn : serial_numbers ) {
            QThread* controller_thread =QThread::create([address, sn, n_motors]() {
                try {
                  exoskeleton::core::RedisSingleMotorController controller(address, sn, n_motors);
                  controller.loop();
                } catch (std::exception const& e) {
                    std::cerr << address << " [ERROR] Exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << address << " [ERROR] Exception" << std::endl;
                }
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