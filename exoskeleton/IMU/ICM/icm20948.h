#pragma once
#include <cstdint>
#include "mcp2221.h"
#include "../sensorfusion/Madgwick.h"
#include "threadsafe_queue.h"
#include "ImuSample.h"
#include <fstream>
#include <filesystem>
#include <format>

inline int ICM20948_SPEED = 100000; //Speed in HZ
inline int ICM20948_ACCELGYRO_SAMPLERATE = 75;

inline uint8_t ICM20948_ADDRESS = 0x69;
inline uint8_t ICM20948_WHOAMI = 0x00;
inline uint8_t WHOAMI_RESULT = 0xEA;
inline uint8_t ICM20948_BANK_SEL = 0x7F;
//ONLY start reg are needed to write, the rest is read from there ( 6 regs)
inline uint8_t ICM20948_GYRO_START = 0x33;
inline uint8_t ICM20948_ACCEL_START = 0x2D;
inline uint8_t ICM20948_TEMP_START = 0x7F; //TEMP is 2 regs
inline uint8_t ICM20948_PWR_MGT_1 = 0x06; //bank 0
inline uint8_t ICM20948_PWR_MGT_2 = 0x07;
inline uint8_t ICM20948_RESET_BIT = 0x01 << 7; //0x80 reset bit H
inline uint8_t ICM20948_CLKSEL = 0x01; //PLL 1 select + clear wakeupbit
//Sensor config
constexpr uint8_t ICM20948_GYRO_CONFIG_1 = 0x01; //BANK 2!
constexpr uint8_t ICM20948_GYRO_CONFIG_2 = 0x02; //BANK 2!
constexpr uint8_t ICM20948_GYRO_SMPLRT_DIV = 0x00; //BANK 2!
constexpr uint8_t ICM20948_ACCEL_CONFIG_1 = 0x14; //BANK 2!
constexpr uint8_t ICM20948_ACCEL_CONFIG_2 = 0x15; //BANK 2!
constexpr uint8_t ICM20948_ACCEL_SMPLRT_DIV_1 = 0x10; //BANK 2!
constexpr uint8_t ICM20948_ACCEL_SMPLRT_DIV_2 = 0x11; //BANK 2!
constexpr uint8_t ICM20948_TEMP_CONFIG = 0x53; //BANK 2!


//FIFO CONFIG
constexpr uint8_t ICM20948_USER_CTRL = 0x03; //BANK 0! BIT6
constexpr uint8_t ICM20948_FIFO_EN_1 = 0x66; //BANK 0! EXT
constexpr uint8_t ICM20948_FIFO_EN_2 = 0x67; //BANK 0! TEMP, GYRO_X,GYRO_Y,GYRO_Z,ACCEL
constexpr uint8_t ICM20948_FIFO_RST = 0x68; //BIT [0:4]
constexpr uint8_t ICM20948_FIFO_MODE = 0x69; //BIT [0:4]
constexpr uint8_t ICM20948_FIFO_COUNTH = 0x70; //FIFO CNT 8:12
constexpr uint8_t ICM20948_FIFO_COUNTL = 0x71; //FIFO_CNT 0:7
constexpr uint8_t ICM20948_FIFO_RW = 0x72; //FIFO_RW

//Factory offsets
constexpr uint8_t ICM20948_XA_OFFS_H = 0x14; // BANK 1!! 6 registers, H then L. H[14:7] L[6:0]. L's LSB bit is reserved
constexpr uint8_t ICM20948_XG_OFFS_H = 0x03; // BANK 2!! 6 registers, H then L. H[15:8] L[7:0]. L's LSB bit is reserved
constexpr float ICM20948_GYRO_OFFS_STPSIZE = 0.0305;

//AK09916 magnetometer settings
constexpr uint8_t ICM20948_INT_PIN_CFG = 0x0F; //BANK 0, Bit1 bypass EN
constexpr uint8_t ICM20948_I2C_MST_CTRL = 0x01;
//BANK 3!, I2C_MST_CLK[3:0] Sets I2C master clock frequency (7),4 I2C_MST_P_NSR = 1 stop btwn reads
constexpr uint8_t ICM20948_BIT_I2C_SLV2_REG = 0x0C; //BANK 3,I2C_SLV2_REG, na ezt már nem értem
constexpr uint8_t AK09916_REG_WHO_AM_I = 0x01;
constexpr uint8_t AK09916_EXPECTED_WHO_AM_I = 0x09;
constexpr uint8_t AK09918_CNTL2 = 0x31; //read mode setup 0x08
constexpr uint8_t AK09918_CNTL3 = 0x32; //B0 = 1 SRTS Soft reset, bit auto clears
//MAGNETOMETER passthrough to FIFO confused. DO I need toset another slv???
constexpr uint8_t ICM20948_I2C_SLV0_ADDR = 0x03; //BANK3, AK09916 addr | bit7:read
constexpr uint8_t ICM20948_I2C_SLV0_REG = 0x04; //BANK3, AK099116 read begin
constexpr uint8_t ICM20948_I2C_SLV0_CTRL = 0x05; //BANK3, setup, complicated, see datasheet


//GYRO_CONFIG_1 LSB= filter on/off, bit 1-2: sens, bit 3-5 filter setup (fingom sincs mit jelentenek de legyen 3)
struct GyroStruct {
    uint8_t regValue;
    float scale;
};

constexpr GyroStruct GYRO_LOW = {0b00110101, 4.0 / 131}; // 1000 dps
constexpr GyroStruct GYRO_MID = {0b00110011, 2.0 / 131.0}; // 500 dps
constexpr GyroStruct GYRO_HIGH = {0b00110001, 1.0 / 131.0}; // 250 dps -> best accuracy
//GYRO_CONFIG_1 LSB= filter on/off, bit 1-2: sens, bit 3-5 filter setup (fingom sincs mit jelentenek de legyen 3)
struct AccelStruct {
    uint8_t regValue;
    float scale;
};

constexpr AccelStruct ACCEL_LOW = {0b00110101, 8.0 / 32768.0}; // +- 8g
constexpr AccelStruct ACCEL_MID = {0b00110011, 4.0 / 32768.0}; // +- 4g
constexpr AccelStruct ACCEL_HIGH = {0b00110001, 2.0 / 32768.0}; // +- 2g -> best accuracy

//HELPER
constexpr float PI = 3.14159265358979323846f;
constexpr float RAD2DEG = 180.0f / PI;
constexpr float DEG2RAD = PI / 180.0f;

//FIFO
// inline uint8_t FIFO_PACKET_SIZE = 12;
// inline uint8_t FIFO_PACKET_MULT = 10;
// inline uint16_t FIFO_MAX_SIZE = 4096;
// inline uint16_t FIFO_BURST_SIZE = FIFO_PACKET_SIZE * FIFO_PACKET_MULT;
// inline uint16_t FIFO_COUNT_THRES = FIFO_BURST_SIZE * 3;

struct IMUConfig {
    //FIFO
    uint8_t FIFO_PACKET_SIZE = 12;
    uint8_t FIFO_PACKET_MULT = 5;
    uint8_t FIFO_PACKET_MULT_HIGH = 10;
    uint16_t FIFO_MAX_SIZE = 4096;
    uint16_t FIFO_BURST_SIZE = FIFO_PACKET_SIZE * FIFO_PACKET_MULT;
    uint16_t FIFO_BURST_SIZE_HIGH = FIFO_PACKET_SIZE * FIFO_PACKET_MULT_HIGH;
    uint16_t FIFO_COUNT_THRES = FIFO_BURST_SIZE * 3;
};

inline extern const IMUConfig def_imu_cfg{};

// inline struct ImuSample {
//     int imu_id{};
//     uint64_t t_ns{}; // monotonic timestamp
//     std::chrono::steady_clock::time_point t_host; // timestamp
//     std::vector<float> accel= std::vector<float>(3);
//     std::vector<float> gyro= std::vector<float>(3);
//     std::optional<std::vector<float>> mag= std::vector<float>(3);   // ha enable_mag = true
//     std::optional<std::vector<float>> euler= std::vector<float>(3); // csak preview célra
//     uint32_t seq{}; // növekvő számláló a producerből
// }imu_sample;

inline std::string calibPathTXT() {
    return "../exoskeleton/IMU/Data/calibration/biases.txt";
}

class ICM20948 {
public:
    ICM20948(MCP2221 &mcp, uint8_t addr, int imu_id, IMUConfig cfg_);

    bool Initialize() const;

    bool MAGInitialize() const;

    bool SelectBank(uint8_t bankNum) const;

    std::vector<float> ReadAccelGyro();

    bool EnableSensors(bool temp_en, bool gyro_en, bool accel_en) const;

    bool GyroConfig() const;

    bool AccelConfig() const;

    bool SensorConfig() const;

    bool CalibrateAccelGyroLegacy(uint16_t NumofSamples);

    bool CalibrateAccelGyro(uint16_t NumofSamples);

    bool FIFOConfig() const;

    bool ReadFIFO() const;

    //Thread safe start and stop for producer thread public
    void start(TSQueue<ImuSample> &out) { Start(out, cfg); }

    void stop();


    bool ReadDummyFIFO(int numofelements) const;

    bool FlushFIFO(int numofelements, int waittime) const;

    void ReadExtSlvReg(int size) const;

    std::vector<float> ReadGyroOffsets() const;

    void WriteGyroOffsets(const std::vector<float> &gyroOffsets) const;

    uint16_t AccelSampleRateSet(float sampleRate) const;

    uint16_t GyroSampleRateSet(float sampleRate) const; // Set Gyro Sample rate (reading rate in Hz)
    static int16_t MergeHL(uint8_t H, uint8_t L);

    bool saveCalibrationAsTxt(const std::string &path);

    bool loadCalibrationfromTxt(const std::string &path);

    bool setZeroing() {
        set_zero = !set_zero;
        just_zeroed = true;
        return set_zero;
    }


    //~ICM20948();
private:
    MCP2221 &mcp;
    uint8_t address;
    IMUConfig cfg;
    int imu_id_;
    GyroStruct gyroconfig = GYRO_HIGH;
    AccelStruct accelconfig = ACCEL_HIGH;
    std::array<float, 3> EulerOffset = {0.0, 0.0, 0.0};
    std::array<float, 3> gyrobias = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> gyrobias_stored = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> accelbias = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> accelbias_stored = {0.0f, 0.0f, 0.0f};
    float calcbeta;
    //for threading
    std::jthread worker_;
    bool set_zero = false, just_zeroed = false;
    //functions
private:
    bool ReadFIFOSize(uint16_t &FIFOCount);

    void ProducerLoop(const std::stop_token &st, TSQueue<ImuSample> *out, IMUConfig cfg);

    //internal start for the producerloop
    void Start(TSQueue<ImuSample> &out, const IMUConfig &cfg);
};
