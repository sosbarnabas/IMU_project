cmake_minimum_required(VERSION 3.28.3)
project(test)

set(CMAKE_CXX_STANDARD 20)

#set(Qt6_DIR "C:/Qt/6.9.0/mingw_64/lib/cmake/Qt6")

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

add_executable(MOTOR motor.cpp)

target_link_libraries(MOTOR
    PRIVATE
        Qt6::Core
        Qt6::SerialPort
)