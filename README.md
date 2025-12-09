# IMU Integration -- Change Summary and Technical Overview

This document describes the changes introduced in the
**`IMU_integration`** branch relative to `main`.\
It explains the new IMU system, summarizes the architectural
modifications, and provides build/run instructions along with key design
decisions.

## 1. Overview of the New IMU Integration

The `IMU_integration` branch introduces a complete inertial-measurement
pipeline into the exoskeleton core.

-   Support for **ICM-20948** 9-DOF IMU sensor\
-   **MCP2221 USB--I²C** communication\
-   **Madgwick** sensor-fusion\
-   IMU orientation tied to motor control, exercise logic, logging, and
    Redis

## 2. New IMU Subsystem

### 2.1 Hardware Interface Layer

-   `exoskeleton/IMU/MCP/mcp2221.cpp/.h`
-   `exoskeleton/IMU/lib/*`

Implements MCP2221-based USB→I²C communication.

### 2.2 IMU Sensor Driver

-   `icm20948.cpp/.h`
-   Calibration files
-   Thread-safe IMU sampling

### 2.3 Sensor Fusion

-   Madgwick AHRS implementation for real-time orientation.

## 3. Core System Changes

### 3.1 Major Modules Added/Modified

-   `Control.cpp/.h` -- IMU-driven control extensions\
-   `ExerciseControl.cpp/.h` -- IMU-based exercise framework\
-   `RedisSingleIMUController.cpp/.h` -- Redis IMU publisher\
-   `DataLogger.cpp/.h` -- unified IMU + motor telemetry\
-   Mock controllers and tests added

## 4. Build Instructions

### Requirements

-   Windows\
-   CMake, C++17\
-   MCP2221 drivers

### Build

    mkdir build
    cd build
    cmake ..
    cmake --build . --config Release

## 5. Important Design Decisions

-   IMU pipeline decoupled using queues\
-   Madgwick filter chosen for stability\
-   Unified logging format\
-   Redis used as main communication hub\
-   Mock layers for hardware-free testing

## 6. Summary

This branch adds full IMU integration, control extensions, logging,
Redis support, and test utilities.\
It establishes the foundation for advanced sensor-driven exoskeleton
features.
