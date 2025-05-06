cmake_minimum_required(VERSION 3.30.5)
project(exoskeleton_core_cpp_local)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

add_executable(redistest demo/redis/redistest.cpp)

#cmake_path(GET CMAKE_SOURCE_DIR PARENT_PATH PARENT_DIR)
#add_executable(REDIS_TEST ${PARENT_DIR}/exoskeleton-core-cpp/tests/redis_test.cpp
add_executable(REDIS_TEST exoskeleton/tests/test.cpp
        exoskeleton/redis/RedisFacade.cpp
        exoskeleton/settings/Settings.cpp
        exoskeleton/settings/UserParams.cpp
        exoskeleton/redis/RedisTools.cpp)

target_link_libraries(REDIS_TEST PRIVATE Qt6::Core Qt6::SerialPort)

target_link_libraries(redistest PRIVATE redis++ hiredis)

target_link_libraries(REDIS_TEST PRIVATE redis++ hiredis)

