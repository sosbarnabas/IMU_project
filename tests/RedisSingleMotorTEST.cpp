// test_RedisSingleMotorController.cpp
#include "../exoskeleton/redis/RedisSingleMotorController.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include <chrono>

// Fake or mock port for testing when no real hardware is connected
class FakeMultiPortExoMotors {
public:
    void enable(int idx) { std::cout << "[FakeMotor] enable(" << idx << ")\n"; }
    void disable(int idx) { std::cout << "[FakeMotor] disable(" << idx << ")\n"; }
    std::vector<std::string> read() {
        std::cout << "[FakeMotor] read()\n";
        return {"fake_data"};
    }
};

int main() {
        std::cout << "[DEBUG] Setting up fake controller without real port...\n";

        int address = 1;
        std::string port = "FAKE_PORT";  // dummy placeholder

        RedisSingleMotorController controller(address, port);
        controller.loop();
    return 0;
}