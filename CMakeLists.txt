cmake_minimum_required(VERSION 3.20)
project(IMU_project)

set(CMAKE_CXX_STANDARD 23)
include_directories(sensorfusion)
include_directories(lib)
include_directories(MCP)
include_directories(ICM)
include_directories(include)
# include_directories(exoskeleton)
# include_directories(exoskeleton/core)
# include_directories(exoskeleton/settings)
# include_directories(exoskeleton/motor)


add_executable(IMU_project main.cpp
        MCP/mcp2221.cpp
        ICM/icm20948.cpp
        #ICM/i_c_m20948_driver.cpp
        sensorfusion/Madgwick.cpp
        include/sample.h
        include/threadsafe_queue.h
        # exoskeleton/core/RedisSingleIMUController.cpp
        # exoskeleton/core/RedisSingleIMUController.h
        # exoskeleton/core/RedisTools.cpp
        # exoskeleton/core/RedisTools.h
)



# Link the MCP2221 import library
target_link_libraries(IMU_project
        ${CMAKE_SOURCE_DIR}/lib/mcp2221_dll_um_x64.lib
)


