cmake_minimum_required(VERSION 3.30.5)
project(exoskeleton_core_cpp_local VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
#add_compile_options(/O2)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)
find_package(nlohmann_json CONFIG REQUIRED)
add_executable(redistest demo/redis/redistest.cpp)

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
        exoskeleton/multiport/RedisSingleMotorController.cpp
        exoskeleton/motor/motor.cpp
        exoskeleton/multiport/MultiPortExoMotors.cpp
        exoskeleton/motor/ExoMotorsInterface.cpp
        exoskeleton/core/SinglePortExoMotor.cpp
        exoskeleton/core/RedisSinglePortController.cpp
        exoskeleton/core/redis_backbone.cpp
)

add_executable(READTEST tests/read_test.cpp
                        exoskeleton/motor/motor.cpp)

target_link_libraries(READTEST PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort)
target_link_libraries(REDIS_TEST PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort redis++::redis++ hiredis::hiredis)
target_link_libraries(SINGLEPORT PRIVATE Qt6::Core Qt6::CorePrivate Qt6::SerialPort redis++::redis++ hiredis::hiredis nlohmann_json::nlohmann_json)
target_link_libraries(REDIS_TEST PRIVATE redis++::redis++ hiredis::hiredis)

