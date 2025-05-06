cmake_minimum_required(VERSION 3.30)
project(exoskeleton_core_cpp_local)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

add_executable(redistest demo/redis/redistest.cpp)

add_executable(REDIS_TEST ../exoskeleton-core-cpp/tests/redis_test.cpp
        exoskeleton/redis/RedisFacade.cpp
        exoskeleton/settings/Settings.cpp
        exoskeleton/settings/UserParams.cpp
        exoskeleton/redis/RedisTools.cpp)

target_link_libraries(REDIS_TEST PRIVATE Qt6::Core Qt6::SerialPort)

target_link_libraries(redistest PRIVATE redis++ hiredis)
target_include_directories(redistest PRIVATE C:/vcpkg/installed/x64-windows/include)
target_link_directories(redistest PRIVATE C:/vcpkg/installed/x64-windows/lib)

target_link_libraries(REDIS_TEST PRIVATE redis++ hiredis)
target_include_directories(REDIS_TEST PRIVATE C:/vcpkg/installed/x64-windows/include)
target_link_directories(REDIS_TEST PRIVATE C:/vcpkg/installed/x64-windows/lib)