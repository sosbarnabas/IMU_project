#include "../exoskeleton/core/RedisSinglePortController.h"
#include "../exoskeleton/core/RedisFacade.h"
#include "../exoskeleton/core/redis_backbone.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <chrono>
#include <QCoreApplication>
#include <Qthread>

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);

    std::vector<QThread*> threads;
    try {
        using namespace std::chrono_literals;
        std::cout << "[DEBUG] Setting up real controller on actual motor port...\n";

        const auto uri = "tcp://127.0.0.1:6379";
        sw::redis::Redis redis(uri);
        redis.del("exit");

        auto redis_facade = exoskeleton::redis::Facade{uri};
        const auto settings = redis_facade.load_env(true);

        const auto motor_props = settings.motor_props();
        int n_motors = motor_props.size();
        for (const auto& [name, sn, a] : motor_props) {
            std::cout << "[INFO] " << name << " (" << sn  << ", " << a << ")" << std::endl;
        }

        int address = 0;

        threads.push_back(QThread::create([]() {
            exoskeleton::core::RedisBackbone main(4ms);
            main();
        }));

        for (const auto& props : motor_props) {
            QThread* controller_thread =QThread::create([props, n_motors]() {
                try {
                  exoskeleton::core::RedisSinglePortController controller(props, n_motors);
                  controller.loop();
                } catch (std::exception const& e) {
                    std::cerr << props.name << " [ERROR] Exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << props.name << " [ERROR] Exception" << std::endl;
                }
            });
            threads.push_back(controller_thread);
            address++;
        }
        for (QThread* thread : threads) {
            thread->start();
        }
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
    // return 0;
}