cmake_minimum_required(VERSION 3.30.5)
project(exoskeleton_core_cpp_local VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
#add_compile_options(/O2)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)
find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)
find_package(nlohmann_json CONFIG REQUIRED)
FetchContent_Declare(lexy URL https://lexy.foonathan.net/download/lexy-src.zip)
FetchContent_MakeAvailable(lexy)
add_executable(redistest tests/redis_test.cpp)

#cmake_path(GET CMAKE_SOURCE_DIR PARENT_PATH PARENT_DIR)
#add_executable(REDIS_TEST ${PARENT_DIR}/exoskeleton-core-cpp/tests/redis_test.cpp
add_executable(REDIS_TEST tests/Real_RedisSingleMotorController_Test.cpp
        #tests/redis_test.cpp
        exoskeleton/core/RedisFacade.cpp
        exoskeleton/settings/Settings.cpp
        exoskeleton/settings/UserParams.cpp
        exoskeleton/core/RedisTools.cpp
        exoskeleton/multiport/RedisSingleMotorController.cpp
        exoskeleton/motor/motor.cpp
        exoskeleton/multiport/MultiPortExoMotors.cpp
        exoskeleton/motor/ExoMotorsInterface.cpp
        exoskeleton/core/redis_backbone.cpp
)

add_executable(SINGLEPORT tests/SinglePortMotorTEST.cpp
        exoskeleton/core/RedisFacade.cpp
        exoskeleton/settings/Settings.cpp
        exoskeleton/settings/UserParams.cpp
        exoskeleton/core/RedisTools.cpp

        #Logging
        exoskeleton/core/DataLogger.cpp

        exoskeleton/multiport/RedisSingleMotorController.cpp
        exoskeleton/motor/motor.cpp
        exoskeleton/multiport/MultiPortExoMotors.cpp
        exoskeleton/motor/ExoMotorsInterface.cpp
        exoskeleton/core/SinglePortExoMotor.cpp
        exoskeleton/core/RedisSinglePortController.cpp
        exoskeleton/core/redis_backbone.cpp
        exoskeleton/core/commands.cpp
        #IMU files
        exoskeleton/IMU/sensorfusion/Madgwick.cpp
        exoskeleton/IMU/ICM/icm20948.cpp
        exoskeleton/IMU/MCP/mcp2221.cpp
        exoskeleton/core/RedisSingleIMUController.cpp
        #Control
        exoskeleton/core/Control.cpp
        exoskeleton/core/ExerciseControl.cpp

)

add_executable(READTEST tests/read_test.cpp
        exoskeleton/motor/motor.cpp)

add_executable(COMMANDS_TEST tests/commands_test.cpp
        exoskeleton/core/commands.cpp)

add_executable(MOCK_MOTORS_TEST tests/mock_motors_test.cpp
        #mocking
        exoskeleton/mock/MockExoMotors.cpp
        exoskeleton/mock/MockRedisSinglePortController.cpp
        #IMU files
        exoskeleton/IMU/sensorfusion/Madgwick.cpp
        exoskeleton/IMU/ICM/icm20948.cpp
        exoskeleton/IMU/MCP/mcp2221.cpp
        exoskeleton/core/RedisSingleIMUController.cpp

        exoskeleton/core/RedisTools.cpp
        exoskeleton/motor/motor.cpp
        exoskeleton/motor/ExoMotorsInterface.cpp
        exoskeleton/settings/Settings.cpp
        #Logging
        exoskeleton/core/DataLogger.cpp
        #IMU files
        exoskeleton/IMU/sensorfusion/Madgwick.cpp
        exoskeleton/IMU/ICM/icm20948.cpp
        exoskeleton/IMU/MCP/mcp2221.cpp
        exoskeleton/core/RedisSingleIMUController.cpp
        #Control
        exoskeleton/core/Control.cpp
)

target_link_libraries(READTEST PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort)
target_link_libraries(REDIS_TEST PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort redis++::redis++ hiredis::hiredis)
target_link_libraries(SINGLEPORT PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort redis++::redis++ hiredis::hiredis nlohmann_json::nlohmann_json foonathan::lexy)
#target_link_libraries(REDIS_TEST PRIVATE redis++::redis++ hiredis::hiredis)
#target_link_libraries(redistest PRIVATE Qt6::SerialPort redis++::redis++ hiredis::hiredis)
target_link_libraries(COMMANDS_TEST PRIVATE nlohmann_json::nlohmann_json foonathan::lexy)
target_link_libraries(MOCK_MOTORS_TEST PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort redis++::redis++ hiredis::hiredis nlohmann_json::nlohmann_json)

# Include directories for IMU controller
include_directories(exoskeleton/IMU/include)
include_directories(exoskeleton/IMU/ICM)
include_directories(exoskeleton/IMU/MCP)

# Link the MCP2221 import library required by the IMU driver
target_link_libraries(MOCK_MOTORS_TEST PRIVATE ${CMAKE_SOURCE_DIR}/exoskeleton/IMU/lib/mcp2221_dll_um_x64.lib)
target_link_libraries(SINGLEPORT PRIVATE ${CMAKE_SOURCE_DIR}/exoskeleton/IMU/lib/mcp2221_dll_um_x64.lib)

