#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>
#include <chrono>
#include "mcp2221.h"
#include "icm20948.h"
#include "threadsafe_queue.h"
#include "ImuSample.h"

#define ICM2_ADDR 0x68
#define ICM1_ADDR 0x69

//#define MCP2221_LIB 1
int main() {
    MCP2221 mcp2221;
    if (!mcp2221.open()) { return -1; }
    auto begin = std::chrono::steady_clock::now();
    ICM20948 IMU1(mcp2221, ICM1_ADDR, 1, def_imu_cfg);
    if (IMU1.Initialize()) {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
        std::cout << "IMU at address: 0x" << std::hex << ICM1_ADDR << " initialized in " << std::dec << duration.count()
                << " ms." << std::endl;
    } else {
        return -1;
    }
    //IMU1.CalibrateAccelGyroLegacy(1000);
    if (IMU1.FIFOConfig()) {
        if (IMU1.CalibrateAccelGyro(1000)) {
            //if (!IMU1.ReadFIFO()) return -1;
            TSQueue<ImuSample> q1, q2;
            ImuSample _sample;
            IMU1.start(q1);
            while (true) {
                std::cout << "Size: " << q1.size();
                _sample = q1.wait_dequeue();
                std::cout << " ID: " << _sample.imu_id << " seq: " << _sample.seq
                           <<  " roll: " << _sample.euler.at(0) << " fifosize: " <<_sample.fifosize <<std::endl;
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // if (!q1.try_dequeue(_sample)) {
                //     IMU1.stop();
                //     std::cout << "STOPPED" << std::endl;
                //     break;
                // } else {
                //     std::cout << "Sample: ID: " << _sample.imu_id << " seq: " << _sample.seq <<
                //             "roll: " << _sample.euler.at(0) << std::endl;
                // };
            }
        }
    } else { return -1; }


    //IMU1.ReadFactoryOffsets();


    mcp2221.close();

    return 0;
}
