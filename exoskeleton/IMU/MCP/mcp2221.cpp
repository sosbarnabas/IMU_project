#include "mcp2221.h"
#include "../ICM/icm20948.h"
#include <iostream>
#include <chrono>
#include <thread>

#include "../lib/mcp2221_dll_um.h"

MCP2221::MCP2221(): handler(nullptr) {
}

MCP2221::~MCP2221() { close(); }

bool MCP2221::open() {
    std::lock_guard<std::mutex> lock(mtx);
    unsigned int devCount;
    int res = 0;
    res = Mcp2221_GetConnectedDevices(MCP2221_DEF_VID, MCP2221_DEF_PID, &devCount);
    if (res < 0 || devCount == 0) {
        std::cerr << "MCP device count: " << devCount << ", connection result: " << res << std::endl;
        return false;
    }
    Mcp2221_CloseAll();
    wait(100);
    handler = Mcp2221_OpenByIndex(MCP2221_DEF_VID, MCP2221_DEF_PID, MCP2221_INDEX);
    wchar_t serialNum[64];
    res = Mcp2221_GetSerialNumberDescriptor(handler, serialNum);
    if (res < 0) {
        std::cerr << "Can't get serial number" << std::endl;
        return false;
    }
    res = SetSpeed(MCP2221_IIC_SPEED);
    if (res < 0) {
        std::cerr << "Can't get serial number" << std::endl;
        return false;
    }
    GetLastError();
    wait(100);
    std::wcout << "Connected to MCP2221 device: " << serialNum << std::endl;
    return true;
}

bool MCP2221::close() {
    std::lock_guard<std::mutex> lock(mtx);
    if (handler) {
        int res = Mcp2221_Close(handler);
        if (res < 0) {
            std::cerr << "Failed to close MCP2221 handler\n";
            return false;
        }
        handler = nullptr;
    }
    GetLastError();
    return true;
}

int MCP2221::i2cWrite(const unsigned char slaveAddr, std::vector<uint8_t> &data) const {
    std::lock_guard<std::mutex> lock(mtx);
    return (
        Mcp2221_I2cWrite(handler, data.size(), slaveAddr, 1, data.data())
    );
}

int MCP2221::i2cReadOnly(unsigned char slaveAddr, std::vector<uint8_t> &out) const {
    std::lock_guard<std::mutex> lock(mtx);
    int res = 0;
       res = Mcp2221_I2cRead(handler, out.size(), slaveAddr, 1, out.data());
    if (res < 0) {
        std::cerr << "Failed to read MCP2221::i2cReadOnly()\n";
        return res;
    }
    //GetLastError();
    return res;
}

int MCP2221::i2cRead(unsigned char slaveAddr, unsigned char reg, std::vector<uint8_t> &out) const {
    std::lock_guard<std::mutex> lock(mtx);
    int res = 0;
    res = Mcp2221_I2cWriteNoStop(handler, 1, slaveAddr, 1, &reg);
    if (res < 0) {
        std::cerr << "Failed to write MCP2221::i2cRead(), " << res << std::endl;
        return res;
    }
    res = Mcp2221_I2cReadRestart(handler, out.size(), slaveAddr, 1, out.data());
    if (res < 0) {
        std::cerr << "Failed to read MCP2221::i2cRead() register: "<<(int)reg<<" error: " <<res << std::endl;
        return res;
    }
    GetLastError();
    return res;
}

int MCP2221::SetSpeed(const unsigned int speed) const {
    //std::lock_guard<std::mutex> lock(mtx);
    return Mcp2221_SetSpeed(handler, speed);
}
uint8_t MCP2221::i2cReadOnlySingle(unsigned char slaveAddr, unsigned char reg) const {
    //std::lock_guard<std::mutex> lock(mtx);
    std::vector<uint8_t> out_data(1);
    if (i2cReadOnly(slaveAddr,out_data) <0){return 0xFF;}
    GetLastError();
    return out_data[0];
}

uint8_t MCP2221::i2cReadSingle(unsigned char slaveAddr, unsigned char reg) const {
    //std::lock_guard<std::mutex> lock(mtx);
    std::vector<uint8_t> out_data(1);
    if (i2cRead(slaveAddr,reg,out_data) <0){return 0xFF;}
    GetLastError();
    return out_data[0];
}

int MCP2221::GetLastError() {
    if (const int err = Mcp2221_GetLastError(); err < 0) {
        std::cerr << "Error detected with code: " << err << std::endl;
        return err;
    }
    return 0;
}

void MCP2221::wait(const int duration) {
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
}
