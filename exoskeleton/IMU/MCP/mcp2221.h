#pragma once
#include <cstdint>
#include <vector>
#include <mutex>


class MCP2221 {
public:
    MCP2221();

    ~MCP2221();

    bool open(); //open the device
    bool close(); //close the connection to avoid conflicts

    int i2cWrite(unsigned char slaveAddr, std::vector<uint8_t> &data) const;

    int i2cReadOnly(unsigned char slaveAddr, std::vector<uint8_t> &out) const;

    uint8_t i2cReadOnlySingle(unsigned char slaveAddr, unsigned char reg) const;

    int SetSpeed(unsigned int speed) const;

    int i2cRead(unsigned char slaveAddr, unsigned char reg, std::vector<uint8_t> &out) const;

    uint8_t i2cReadSingle(unsigned char slaveAddr, unsigned char reg) const;


    int WHOAMI() const;
    static void wait(int duration);
    static int GetLastError() ;

private:
    void *handler;
    mutable std::mutex mtx;

};

#define MCP2221_LIB        1
inline unsigned int MCP2221_DEF_VID = 0x04D8; //Def vendor if, from datasheet
inline unsigned int MCP2221_DEF_PID = 0x00DD; //def. product id from datasheet
inline int MCP2221_IIC_SPEED = 200000; //def. speed
inline unsigned int MCP2221_INDEX = 0; //def. speed
