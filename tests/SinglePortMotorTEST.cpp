#include "../exoskeleton/core/RedisSinglePortController.h"
#include "../exoskeleton/core/RedisSingleIMUController.h"
#include "../exoskeleton/core/RedisFacade.h"
#include "../exoskeleton/core/RedisTools.h"
#include "../exoskeleton/core/redis_backbone.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <chrono>
#include <QCoreApplication>
#include <Qthread>
#include <memory>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::vector<QThread *> threads;
    std::unique_ptr<exoskeleton::core::RedisSingleIMUController> imu_controller;
    MCP2221 mcp2221;

    try
    {
        using namespace std::chrono_literals;
        std::cout << "[DEBUG] Setting up real controller on actual motor port...\n";

        const auto uri = "tcp://127.0.0.1:6379";
        sw::redis::Redis redis(uri);
        redis.del("exit");

        auto redis_facade = exoskeleton::redis::Facade{uri};
        const auto settings = redis_facade.load_env(true);

        const auto motor_props = settings.motor_props();
        int n_motors = motor_props.size();
        for (const auto &[name, sn, a] : motor_props)
        {
            std::cout << "[INFO] " << name << " (" << sn << ", " << a << ")" << std::endl;
            exoskeleton::redis_tools::log(redis, "main", name + " " + sn + " " + std::to_string(a));
        }

        int address = 0;

        threads.push_back(QThread::create([]()
                                          {
            exoskeleton::core::RedisBackbone main(4ms);
            main(); }));

        for (const auto &props : motor_props)
        {
            QThread *controller_thread = QThread::create([&redis, props, n_motors]()
                                                         {
                try {
                  exoskeleton::core::RedisSinglePortController controller(props, n_motors);
                  controller.loop();
                } catch (std::exception const& e) {
                    std::cerr << props.name << " [ERROR] Exception: " << e.what() << std::endl;
                    exoskeleton::redis_tools::log(redis, props.name, e.what(), exoskeleton::redis_tools::LogLevel::error);
                } catch (...) {
                    std::cerr << props.name << " [ERROR] Exception" << std::endl;
                    exoskeleton::redis_tools::log(redis, props.name, "Unknown exception", exoskeleton::redis_tools::LogLevel::error);
                } });
            threads.push_back(controller_thread);
            address++;
        }
        for (QThread *thread : threads)
        {
            thread->start();
        }

        // Initialize and launch IMU controller
        std::cout << "[MAIN] Initializing MCP2221 for IMU...\\n";
        imu_controller = std::make_unique<exoskeleton::core::RedisSingleIMUController>(
            mcp2221,
            uri,
            0 // imu_id = 0
        );
        std::cout << "[MAIN] IMU controller created.\\n";

        QThread *imu_thread = QThread::create([&imu_controller]()
                                              {
            try {
                std::cout << "[IMU] Starting IMU loop in QThread...\\n";
                imu_controller->loop();
                std::cout << "[IMU] IMU loop finished.\\n";
            } catch (const std::exception& e) {
                std::cerr << "[IMU] Exception: " << e.what() << std::endl;
            } });
        threads.push_back(imu_thread);
        imu_thread->start();

        std::cout << "[MAIN] All controller threads launched (motors + IMU).\\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return app.exec();
    // return 0;
}