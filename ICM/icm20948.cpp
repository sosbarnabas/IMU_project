//
// Created by sosba on 2025. 09. 23..
//

#include "icm20948.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <ostream>
#include <thread>
#include <windows.h>

#include <cmath>
//#include <iomanip>


ICM20948::ICM20948(MCP2221 &mcpRef, uint8_t addr, int imu_id, IMUConfig cfg_): mcp(mcpRef), address(addr), cfg(cfg_),
                                                                               imu_id_(imu_id) {
}

bool ICM20948::Initialize() const {
    if (!SelectBank(0)) return false;
    int res = 0;
    if (const uint8_t WHOAMI_reg_val = mcp.i2cReadSingle(address, ICM20948_WHOAMI);
        WHOAMI_reg_val != WHOAMI_RESULT) {
        std::cerr << "Error reading WHOAMI register value: " << std::hex << static_cast<int>(WHOAMI_reg_val) <<
                std::endl;
        return false;
    }
    std::vector<uint8_t> data = {ICM20948_PWR_MGT_1, ICM20948_RESET_BIT};
    res = mcp.i2cWrite(address, data);
    //wait for device fully reset
    mcp.wait(50);
    data = {ICM20948_PWR_MGT_1, ICM20948_CLKSEL};
    res = mcp.i2cWrite(address, data);

    if (!EnableSensors(true, true, true)) { return false; }

    if (!SensorConfig()) return false;
    //if (!MAGInitialize()) return false;
    return true;
}

bool ICM20948::MAGInitialize() const {
    SelectBank(0);
    std::vector<uint8_t> data = {ICM20948_INT_PIN_CFG, 0x02}; //ENABLE BIT
    mcp.i2cWrite(address, data);
    SelectBank(3);
    data = {ICM20948_I2C_MST_CTRL, 0x07}; //datasheet
    mcp.i2cWrite(address, data);
    uint8_t whoami = mcp.i2cReadSingle(ICM20948_BIT_I2C_SLV2_REG, AK09916_REG_WHO_AM_I);
    if (whoami != AK09916_EXPECTED_WHO_AM_I) {
        return false;
    }
    data = {AK09918_CNTL2, 0x08}; //Legyen 8, mert az Mode 4 continous reading
    int res = mcp.i2cWrite(ICM20948_BIT_I2C_SLV2_REG, data);

    mcp.wait(20);
    data = {ICM20948_I2C_SLV0_ADDR, 0b10001100}; //read|0x0C
    mcp.i2cWrite(address, data);
    data = {ICM20948_I2C_SLV0_REG, 0x11}; //0x0C
    mcp.i2cWrite(address, data);
    data = {ICM20948_I2C_SLV0_CTRL, 0b10001000};
    //Enable read, plis read 8 byted, I dont know swap bytes is needed (maybe)
    mcp.i2cWrite(address, data);
    mcp.wait(200);
    return true;
}

int16_t ICM20948::MergeHL(uint8_t high, uint8_t low) {
    return static_cast<int16_t>((high << 8) | low);
}

bool ICM20948::EnableSensors(bool temp_en, bool gyro_en, bool accel_en) const {
    //No header variables used because it is easier for visualization
    //PWR_MGT_2 bits[0:2] = GYRO_DIS, bits[3:5] = ACCEL DIS -> 000 = EN, 111 = DIS
    //PWR_MGT_1 bit 6 = sleep, 0 = wake up, 1 = sleep
    int res = 0;
    uint8_t PWR_MGT1 = mcp.i2cReadSingle(address, ICM20948_PWR_MGT_1);
    //uint8_t PWR_MGT2 = mcp.i2cReadSingle(address, ICM20948_PWR_MGT_2);
    if (temp_en) { PWR_MGT1 &= 0b11111011; } else { PWR_MGT1 |= 0b00000100; }
    uint8_t PWR_MGT2 = 0b00000000;
    if (!gyro_en) { PWR_MGT2 |= 0b00000111; }
    if (!accel_en) { PWR_MGT2 |= 0b00111000; }
    std::vector<uint8_t> data = {ICM20948_PWR_MGT_1, PWR_MGT1};
    res = mcp.i2cWrite(address, data);
    mcp.wait(60);
    data = {ICM20948_PWR_MGT_2, PWR_MGT2};
    res = mcp.i2cWrite(address, data);
    mcp.wait(60);
    if (res < 0) {
        std::cerr << "Error detected with sensor enable: " << res << std::endl;
        return false;
    }

    return true;
}

bool ICM20948::SensorConfig() const {
    SelectBank(2);
    GyroConfig();
    uint16_t gyrosmplrt = GyroSampleRateSet(ICM20948_ACCELGYRO_SAMPLERATE);
    AccelConfig();
    uint16_t accelsmplrt = AccelSampleRateSet(ICM20948_ACCELGYRO_SAMPLERATE);
    SelectBank(0);
    if (int err = mcp.GetLastError(); err != 0) {
        std::cerr << "Error detected with sensor config: " << err << std::endl;
        return false;
    }
    std::cout << "Accel sample rate: " << accelsmplrt << ", gyro sample rate: " << gyrosmplrt << std::endl;
    return true;
}

bool ICM20948::GyroConfig() const {
    std::vector<uint8_t> data = {ICM20948_GYRO_CONFIG_1, gyroconfig.regValue};
    int res = mcp.i2cWrite(address, data);
    if (res < 0) {
        std::cerr << "Error can't write to Gyro_Config: " << res << std::endl;
        return false;
    }
    return true;
}

bool ICM20948::AccelConfig() const {
    std::vector<uint8_t> data = {ICM20948_ACCEL_CONFIG_1, accelconfig.regValue};
    int res = mcp.i2cWrite(address, data);
    if (res < 0) {
        std::cerr << "Error can't write to ACCEL_Config: " << res << std::endl;
        return false;
    }
    return true;
}

bool ICM20948::SelectBank(uint8_t bankNum) const {
    if (bankNum < 0 || bankNum > 3) {
        std::cerr << "Invalid bank number" << std::endl;
        return false;
    }
    uint8_t bankVal = bankNum << 4;
    std::vector<uint8_t> bankBuffer = {ICM20948_BANK_SEL, bankVal};
    int res = mcp.i2cWrite(address, bankBuffer);
    if (res < 0) {
        std::cerr << "Failed to write Bank number, " << res << std::endl;
        return false;
    }
    return true;
}

//Loop start, stop and loom implementation
void ICM20948::Start(TSQueue<ImuSample> &out, const IMUConfig &cfg) {
    stop();
    worker_ = std::jthread([this,&out,cfg](const std::stop_token &st) {
        ProducerLoop(st, &out, cfg);
    });
}


void ICM20948::stop() {
    if (worker_.joinable()) worker_.request_stop(), worker_.join();
}

//producer loop helper function
// Read FIFO count
bool ICM20948::ReadFIFOSize(uint16_t &FIFOCount) {
    std::vector<uint8_t> data(2);
    int status = mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
    if (status < 0) { return false; }
    FIFOCount = MergeHL(data.at(0), data.at(1));
    return true;
}

void ICM20948::ProducerLoop(const std::stop_token &st, TSQueue<ImuSample> *out, const IMUConfig cfg) {
    if (out == nullptr) { return; }
    // Base and high multipliers from cfg
    int pkt_size = cfg.FIFO_PACKET_SIZE; // bytes/packet
    int pkt_mult_base = cfg.FIFO_PACKET_MULT; // e.g. 10
    int pkt_mult_high = cfg.FIFO_PACKET_MULT_HIGH; // e.g. 15
    int pkt_mult = pkt_mult_base;

    // Threshold multiplier to decide “very full”
    int fifo_thres_mult = 8; // hysteresis gap

    // Derived sizes (kept in sync via set_mult)
    int fifo_read_size = pkt_mult * pkt_size; // bytes to read per burst
    int burst_size = fifo_read_size; // alias for clarity

    // Buffers
    std::vector<uint8_t> fifo_buffer; // read buffer
    fifo_buffer.resize(fifo_read_size);

    //flags
    bool overflow = false, underflow = false;

    //Acceleration normalization
    float sum_accel = 0.0, normalized_accel = 0.0;


    // Helper to switch between 10x and 15x safely
    auto set_mult = [&](int m) {
        pkt_mult = m;
        fifo_read_size = pkt_mult * pkt_size;
        burst_size = fifo_read_size;
        fifo_buffer.resize(fifo_read_size);
        std::cout << "Fifo multiplier set to " << pkt_mult << " ,burst size "<< burst_size <<std::endl;
    };

    uint16_t fifo_size = 0;
    uint32_t seq = 0;
    SelectBank(0);
    //main loop
    while (!st.stop_requested()) {
        if (!ReadFIFOSize(fifo_size)) {
            std::cerr << "Can't read FIFO size" << std::endl;
            return;
        }
        // --- Multipliers---
        const bool using_high = (pkt_mult == pkt_mult_high);
        const uint16_t low_thresh = static_cast<uint16_t>(pkt_size * pkt_mult_base); // ~ one base burst
        const uint16_t high_thresh = static_cast<uint16_t>(pkt_size * pkt_mult_base * fifo_thres_mult); // “very full”

        if (!using_high && fifo_size > high_thresh) {
            set_mult(pkt_mult_high); // drain faster
            overflow = true;
        } else if (using_high && fifo_size < low_thresh) {
            set_mult(pkt_mult_base); // back to normal
            underflow = true;
        } else if (fifo_size < burst_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        mcp.i2cRead(address, ICM20948_FIFO_RW, fifo_buffer);

        for (int packet = 0; packet < pkt_mult; packet++) {
            ImuSample sample{};
            const auto t_host = std::chrono::steady_clock::now();
            for (int i = 0; i < sample.accel.size(); i++) {
                size_t idx = pkt_size * packet + i * 2;
                int16_t raw = MergeHL(fifo_buffer[idx], fifo_buffer[idx + 1]);
                const float accel_val = raw * accelconfig.scale;
                sum_accel += accel_val * accel_val;
                sample.accel.at(i) = accel_val;
            }
            for (int i = 0; i < sample.gyro.size(); i++) {
                size_t idx = pkt_size * packet + (i + 3) * 2;
                int16_t raw = MergeHL(fifo_buffer[idx], fifo_buffer[idx + 1]);
                const float gyro_val = raw * gyroconfig.scale;
                sample.gyro.at(i) = gyro_val;
            }

            normalized_accel = sqrtf(sum_accel);
            for (int i = 0; i < sample.accel.size(); i++) {
                sample.accel.at(i) /= normalized_accel;
            }

            //Normal use
            MadgwickAHRSupdateIMU(sample.gyro.at(0) * DEG2RAD, sample.gyro.at(1) * DEG2RAD,
                                  sample.gyro.at(2) * DEG2RAD,
                                  sample.accel.at(0), sample.accel.at(1), sample.accel.at(2));
            //we want to cancel out gimbal lock on IMU pitch/Y axis to mount it perpendicular so we swap x and y, (maybe -1*z?)
            // MadgwickAHRSupdateIMU(AccelGyroData[4] * DEG2RAD, AccelGyroData[3] * DEG2RAD,
            //                      -AccelGyroData[5] * DEG2RAD,
            //                      AccelGyroData[1], AccelGyroData[0], -AccelGyroData[2], freq_per_sample);


            std::array<float, 3> euler_;
            QuaternionsToEulerAngles(euler_);
            //eulerAngles(euler);
            //eulerAnglesRPswap(euler);
            // Convert to degrees
            if (just_zeroed) {
                EulerOffset[0] = euler_[0];
                EulerOffset[1] = euler_[1];
                EulerOffset[2] = euler_[2];
                just_zeroed = false;
            }
            for (int e = 0; e < euler_.size(); e++) {
                if (set_zero) {
                    sample.euler.at(e) = (euler_.at(e) - EulerOffset.at(e)) * RAD2DEG;
                } else {
                    sample.euler.at(e) = euler_.at(e) * RAD2DEG;
                }
            }


            //Sample write
            sample.imu_id = imu_id_;
            sample.seq = seq++;
            sample.t_host = t_host;
            sample.fifo_overflow = overflow;
            sample.fifo_underflow = underflow;
            sample.fifosize = fifo_size;
            sample.mag = {0.0, 0.0, 0.0};
            sample.mag_ok = 0;
            out->enqueue(sample);
        }
        overflow = false;
        underflow = false;

    }
}


std::vector<float> ICM20948::ReadAccelGyro() {
    std::vector<float> AccelGyroData(6); // [0:2] accel, [3:5] gyro
    std::vector<uint8_t> out(12); //2 byte per sensor per axis
    int res = mcp.i2cRead(address, ICM20948_ACCEL_START, out);
    if (res < 0) {
        std::cerr << "Failed to read gyro-acceleration register: " << res << std::endl;
    }

    for (int i = 0; i < out.size() / 2; i++) {
        float rawData = MergeHL(out.at(i * 2), out.at(i * 2 + 1));
        if (i < 3) {
            AccelGyroData.at(i) = rawData * accelconfig.scale;
        } else { AccelGyroData.at(i) = rawData * gyroconfig.scale; }
    }


    return AccelGyroData;
}

bool ICM20948::CalibrateAccelGyroLegacy(uint16_t NumofSamples) {
    std::array<float, 3> sumGyro = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> sumAccel = {0.0f, 0.0f, 0.0f};
    float sumAccelMean = 0;
    float AccelSquare = 0;
    float sumGyroMean = 0;
    float GyroMean = 0;
    double ax_sq = 0, ay_sq = 0, az_sq = 0;
    int sampleCount = 0;
    std::vector<uint8_t> data(2);
    uint16_t fifoCount = 0;
    int underflowCount = 0;
    int overflowCount = 0;
    bool fifoStopped = false;


    std::cout << "Calibrating in 2 seconds..." << std::endl;
    FlushFIFO(300, 1000);
    std::cout << "Calibrating..." << std::endl;
    //return true;
    auto calibstart = std::chrono::high_resolution_clock::now();
    while (sampleCount < NumofSamples) {
        // Read FIFO count
        mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
        fifoCount = MergeHL(data.at(0), data.at(1));
        //std::cout << "Calib FIFO size: " << fifoCount << "sampleCount: " << sampleCount << std::endl;
        if (fifoCount <= cfg.FIFO_BURST_SIZE) {
            std::cout << "FIFO underflow." << std::endl;
            if (++underflowCount >= 10) {
                std::cout << "FIFO empty(" << fifoCount << "), underflowcount: " << underflowCount << std::endl;
                break;
            }
            continue;
        }

        if (fifoCount >= cfg.FIFO_MAX_SIZE - cfg.FIFO_BURST_SIZE) {
            std::cout << "FIFO overflow." << std::endl;
            if (++overflowCount >= 10) {
                std::cout << "FIFO overflow(" << fifoCount << "), overflowcount: " << overflowCount << std::endl;
                break;
            }
        }

        // Adjust sample rate if needed
        if (fifoCount <= cfg.FIFO_BURST_SIZE * 2.5) {
            //mcp.wait(1500);
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Underflow" << std::endl;
            continue;
        }
        if (fifoCount >= cfg.FIFO_MAX_SIZE - cfg.FIFO_BURST_SIZE * 4) {
            std::vector<uint8_t> fifoendata = {ICM20948_FIFO_EN_2, 0x00};
            mcp.i2cWrite(address, fifoendata);
            fifoStopped = true;
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Overflow, fifo count: " << fifoCount << std::endl;
        }
        if (fifoStopped) {
            if (fifoCount <= cfg.FIFO_MAX_SIZE / 2) {
                //Enable Accel, Gyro_X_Y_Z but disable LSB (TEMP). MAybe enable for temp compensation TODO!
                std::vector<uint8_t> fifoendata = {ICM20948_FIFO_EN_2, 0b00011110};
                mcp.i2cWrite(address, fifoendata);
                auto start = std::chrono::high_resolution_clock::now();
                std::cout << "FIFO Enabled, at size " << fifoCount << std::endl;
            }
        }

        if (fifoCount >= cfg.FIFO_BURST_SIZE * 2) {
            std::vector<uint8_t> fifoData(cfg.FIFO_BURST_SIZE);
            auto readstart = std::chrono::high_resolution_clock::now();
            mcp.i2cRead(address, ICM20948_FIFO_RW, fifoData);

            // Parse packets
            for (int p = 0; p < cfg.FIFO_PACKET_MULT; p++) {
                std::vector<float> AccelGyroData(7); // [0:2] accel, [3:5] gyro
                for (int i = 0; i < 6; i++) {
                    int idx = p * cfg.FIFO_PACKET_SIZE + i * 2;
                    float rawData = MergeHL(fifoData[idx], fifoData[idx + 1]);
                    if (i < 3) {
                        AccelGyroData.at(i) = (rawData * accelconfig.scale); // g
                        sumAccel.at(i) += AccelGyroData.at(i);
                        //AccelSquare += AccelGyroData.at(i) * AccelGyroData.at(i);
                        //std::cout << "Raw accel: " << rawData << "  scaled: " << AccelGyroData.at(i) << std::endl;
                    } else {
                        AccelGyroData.at(i) = (rawData * gyroconfig.scale); // dps
                        sumGyro.at(i - 3) += AccelGyroData.at(i);
                        //GyroMean += AccelGyroData.at(i) * AccelGyroData.at(i);
                        //std::cout << "Raw gyro: " << rawData << "  scaled: " << AccelGyroData.at(i) << std::endl;
                    }
                }
                //Normal
                ax_sq += AccelGyroData.at(3) * AccelGyroData.at(3);
                ay_sq += AccelGyroData.at(4) * AccelGyroData.at(4);
                az_sq += AccelGyroData.at(5) * AccelGyroData.at(5);
                //Roll pitch swap
                // gx_sq += AccelGyroData.at(4) * AccelGyroData.at(4);
                // gy_sq += AccelGyroData.at(3) * AccelGyroData.at(3);
                // gz_sq += AccelGyroData.at(5) * AccelGyroData.at(5);
                sampleCount++;
                //std::cout<< "Accel squared - " << std::sqrt(AccelMean) << std::endl;

                //AccelSquare = 0;
            }
            //std::cout << "FIFO_count: " << fifoCount << std::endl;
        }
    }

    for (int i = 0; i < sumAccel.size(); i++) {
        sumGyro.at(i) /= static_cast<float>(sampleCount);
        sumAccel.at(i) /= static_cast<float>(sampleCount);
    }
    sumAccel.at(2) -= 1;
    accelbias = sumAccel;
    accelbias_stored = sumAccel;
    gyrobias = sumGyro;
    //gyrobias_stored = sumGyro;
    // WriteGyroOffsets(gyrobias);
    // ReadGyroOffsets();
    auto calibend = std::chrono::high_resolution_clock::now();
    double calibduration = std::chrono::duration_cast<std::chrono::seconds>(calibend - calibstart).count();
    std::cout << "AccelBias: " << sumAccel.at(0) << " " << sumAccel.at(1) << " " << sumAccel.at(2) << " GyroBias: "
            << sumGyro.at(0) << " " << sumGyro.at(1) << " " << sumGyro.at(2) << std::endl;

    return true;
}

uint16_t ICM20948::GyroSampleRateSet(float sampleRate) const {
    // Calculation: sampleRate= 1125/(1+GYRO_SMPLRT_DIV)Hz where GYRO_SMPLRT_DIV is 0, 1, 2,…255
    //GYRO_SMPLRT_DIV = 1125/sampleRate - 1
    float _gyrosmplrtdiv = 1125 / sampleRate - 1;
    if (_gyrosmplrtdiv > 255) { _gyrosmplrtdiv = 255; }
    if (_gyrosmplrtdiv < 0) { _gyrosmplrtdiv = 0.0f; }
    std::vector<uint8_t> data = {ICM20948_GYRO_SMPLRT_DIV, static_cast<uint8_t>(_gyrosmplrtdiv)};
    mcp.i2cWrite(address, data);
    return 1125 / (static_cast<uint8_t>(_gyrosmplrtdiv) + 1);
}

uint16_t ICM20948::AccelSampleRateSet(float sampleRate) const {
    // Calculation: 1125/(1+ACCEL_SMPLRT_DIV)Hz where ACCEL_SMPLRT_DIV is 0, 1, 2,…4095
    //ACCEL_SMPLRT_DIV = 1125/sampleRate - 1
    float accelsmplrtdiv = 1125 / sampleRate - 1;
    uint16_t _accelsmplrtdiv = static_cast<uint16_t>(accelsmplrtdiv);
    if (_accelsmplrtdiv > 4095) { _accelsmplrtdiv = 4095; }
    if (_accelsmplrtdiv < 0) { _accelsmplrtdiv = 0.0f; }
    //Accel_div [0:7] - DIV_2; Accel_div [8:11] - DIV_1
    std::vector<uint8_t> data = {ICM20948_ACCEL_SMPLRT_DIV_1, static_cast<uint8_t>(_accelsmplrtdiv >> 8)};
    mcp.i2cWrite(address, data);
    data = {ICM20948_ACCEL_SMPLRT_DIV_2, static_cast<uint8_t>(_accelsmplrtdiv & 0xFF)};
    mcp.i2cWrite(address, data);
    return 1125 / ((static_cast<uint8_t>(accelsmplrtdiv)) + 1);
}

//safe calibration saver
bool ICM20948::saveCalibrationAsTxt(const std::string &stringpath) {
    const std::filesystem::path path(stringpath);
    std::filesystem::path parent = path.parent_path();
    if (!std::filesystem::exists(parent)) { std::filesystem::create_directories(parent); }
    std::ofstream ofs;
    ofs.open(path, std::ofstream::out);
    if (!ofs) return false;
    ofs << "gyro_bias: " << gyrobias_stored.at(0) << ';' << gyrobias_stored.at(1) << ';' << gyrobias_stored.at(2) <<
            '\n';
    ofs << "accel_bias: " << accelbias_stored.at(0) << ';' << accelbias_stored.at(1) << ';' << accelbias_stored.at(2) <<
            '\n';

    ofs.close();
    gyrobias = gyrobias_stored;
    accelbias = accelbias_stored;
    return true;
}

//safe calibration data loader
bool ICM20948::loadCalibrationfromTxt(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) return false;

    std::string line;
    auto parseLine = [](const std::string &src, const std::string &key, std::array<float, 3> &out) -> bool {
        if (src.rfind(key, 0) != 0) return false; // must start with key
        std::string values = src.substr(key.size());
        std::replace(values.begin(), values.end(), ';', ' ');
        std::istringstream iss(values);
        return static_cast<bool>(iss >> out[0] >> out[1] >> out[2]);
    };

    bool gotG = false, gotA = false;
    while (std::getline(ifs, line)) {
        if (!gotG) gotG = parseLine(line, "gyro_bias:", gyrobias_stored);
        if (!gotA) gotA = parseLine(line, "accel_bias:", accelbias_stored);
    }
    ifs.close();

    gyrobias = gyrobias_stored;
    accelbias = accelbias_stored;
    return gotG && gotA;
}


bool ICM20948::CalibrateAccelGyro(uint16_t NumofSamples) {
    std::array<float, 3> sumGyro = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> sumAccel = {0.0f, 0.0f, 0.0f};
    float sumAccelMean = 0;
    float AccelSquare = 0;
    float sumGyroMean = 0;
    float GyroMean = 0;
    double ax_sq = 0, ay_sq = 0, az_sq = 0;
    int sampleCount = 0;
    std::vector<uint8_t> data(2);
    uint16_t fifo_size = 0;
    // Base and high multipliers from cfg
    int pkt_size = cfg.FIFO_PACKET_SIZE; // bytes/packet
    int pkt_mult_base = cfg.FIFO_PACKET_MULT; // e.g. 10
    int pkt_mult_high = cfg.FIFO_PACKET_MULT_HIGH; // e.g. 15
    int pkt_mult = pkt_mult_base;

    // Threshold multiplier to decide “very full”
    int fifo_thres_mult = 8; // hysteresis gap

    // Derived sizes (kept in sync via set_mult)
    int fifo_read_size = pkt_mult * pkt_size; // bytes to read per burst
    int burst_size = fifo_read_size; // alias for clarity

    // Buffers
    std::vector<uint8_t> fifo_buffer; // read buffer
    fifo_buffer.resize(fifo_read_size);
    // Helper to switch between 10x and 15x safely
    auto set_mult = [&](int m) {
        pkt_mult = m;
        fifo_read_size = pkt_mult * pkt_size;
        burst_size = fifo_read_size;
        fifo_buffer.resize(fifo_read_size);
    };

    std::cout << "Calibrating in 2 seconds..." << std::endl;
    FlushFIFO(300, 800);
    std::cout << "Calibrating..." << std::endl;
    //return true;
    auto calibstart = std::chrono::high_resolution_clock::now();
    while (sampleCount < NumofSamples) {
        if (!ReadFIFOSize(fifo_size)) {
            std::cerr << "Can't read FIFO size" << std::endl;
            return false;
        }
        // --- Multipliers---
        const bool using_high = (pkt_mult == pkt_mult_high);
        const uint16_t low_thresh = static_cast<uint16_t>(pkt_size * pkt_mult_base); // ~ one base burst
        const uint16_t high_thresh = static_cast<uint16_t>(pkt_size * pkt_mult_base * fifo_thres_mult); // “very full”

        if (!using_high && fifo_size > high_thresh) {
            set_mult(pkt_mult_high); // drain faster
        } else if (using_high && fifo_size < low_thresh) {
            set_mult(pkt_mult_base); // back to normal
        } else if (fifo_size < burst_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        mcp.i2cRead(address, ICM20948_FIFO_RW, fifo_buffer);

        for (int packet = 0; packet < pkt_mult; packet++) {
            ImuSample sample{};
            const auto t_host = std::chrono::steady_clock::now();
            for (int i = 0; i < sample.accel.size(); i++) {
                size_t idx = pkt_size * packet + i * 2;
                int16_t raw = MergeHL(fifo_buffer[idx], fifo_buffer[idx + 1]);
                const float accel_val = raw * accelconfig.scale;
                sumAccel.at(i) += accel_val;
            }
            for (int i = 0; i < sample.gyro.size(); i++) {
                size_t idx = pkt_size * packet + (i + 3) * 2;
                int16_t raw = MergeHL(fifo_buffer[idx], fifo_buffer[idx + 1]);
                const float gyro_val = raw * gyroconfig.scale;
                sumGyro.at(i) += gyro_val;
            }
            sampleCount++;
        }
        std::cout << "Calibration FIFO size: " << fifo_size << "samples: " << sampleCount << std::endl;
    }
    for (int i = 0; i < sumAccel.size(); i++) {
        sumGyro.at(i) /= static_cast<float>(sampleCount);
        sumAccel.at(i) /= static_cast<float>(sampleCount);
    }

    sumAccel.at(2) -= 1;
    accelbias = sumAccel;
    accelbias_stored = sumAccel;
    gyrobias = sumGyro;
    gyrobias_stored = sumGyro;
    // WriteGyroOffsets(gyrobias);
    // ReadGyroOffsets();
    auto calibend = std::chrono::high_resolution_clock::now();
    double calibduration = std::chrono::duration_cast<std::chrono::seconds>(calibend - calibstart).count();
    std::cout << "AccelBias: " << sumAccel.at(0) << " " << sumAccel.at(1) << " " << sumAccel.at(2) << " GyroBias: "
            << sumGyro.at(0) << " " << sumGyro.at(1) << " " << sumGyro.at(2) << std::endl;
    saveCalibrationAsTxt(calibPathTXT());

    return true;
}

bool ICM20948::FIFOConfig() const {
    SelectBank(0);
    mcp.wait(50);
    std::vector<uint8_t> data(2);
    //ENABLE FIFO
    data = {ICM20948_USER_CTRL, 0b01100000}; // SET FIFO ENABLE TO 1, adn enable i2x_mster
    mcp.i2cWrite(address, data);
    //FIFO_RESET[4:0] S/W FIFO reset. Assert and hold to set FIFO size to 0. Assert and de-assert to reset FIFO.
    //data = {ICM20948_FIFO_RST, 0b00011111};
    data = {ICM20948_FIFO_RST, 0b00000001};
    mcp.i2cWrite(address, data);
    //FIFO_RESET[4:0] S/W FIFO reset. Assert and hold to set FIFO size to 0. Assert and de-assert to reset FIFO.
    data = {ICM20948_FIFO_RST, 0b00000000};
    mcp.i2cWrite(address, data);
    // data = {ICM20948_USER_CTRL, 0b01000000}; // ENABLE FIFO
    // mcp.i2cWrite(address, data);
    //FIFO_MODE[4:0]; 1 – Snapshot. When set to ‘1’, when the FIFO is full, additional writes will not be written to FIFO. When set to ‘0’, when the FIFO is full, additional writes will be written to the FIFO, replacing the oldest data.
    data = {ICM20948_FIFO_MODE, 0b00000001};
    mcp.i2cWrite(address, data);
    //Enable Accel, Gyro_X_Y_Z but disable LSB (TEMP). MAybe enable for temp compensation TODO!
    data = {ICM20948_FIFO_EN_2, 0b00011110};
    mcp.i2cWrite(address, data);


    int err = mcp.GetLastError();
    if (err < 0) {
        std::cerr << "Failed to configure FIFO: " << err << std::endl;
        return false;
    }
    uint8_t res = mcp.i2cReadSingle(address, 0x03);
    //ReadExtSlvReg(8);
    return true;
}

bool ICM20948::FlushFIFO(int numofelements, int waittime) const {
    std::cout << "FIFO flush start" << std::endl;
    mcp.wait(waittime / 2);
    std::vector<uint8_t> data(2);
    //FIFO_RESET[4:0] S/W FIFO reset. Assert and hold to set FIFO size to 0. Assert and de-assert to reset FIFO.
    //data = {ICM20948_FIFO_RST, 0b00011111};
    data = {ICM20948_FIFO_RST, 0b00000001};
    mcp.i2cWrite(address, data);
    //FIFO_RESET[4:0] S/W FIFO reset. Assert and hold to set FIFO size to 0. Assert and de-assert to reset FIFO.
    data = {ICM20948_FIFO_RST, 0b00000000};
    mcp.i2cWrite(address, data);
    mcp.wait(waittime / 2);
    // Read FIFO count
    //mcp.wait(waitTime);
    ReadDummyFIFO(numofelements);
    std::cout << "FIFO flush end" << std::endl;
    return true;
}

bool ICM20948::ReadDummyFIFO(int numofelements) const {
    bool DebugMode = false;
    uint16_t fifoCount = 0;
    std::vector<uint8_t> data(2);
    int iteration = 0;
    while (fifoCount < cfg.FIFO_COUNT_THRES) {
        mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
        fifoCount = MergeHL(data.at(0), data.at(1));
        if (DebugMode) { std::cout << "ReadDummyFIFO buildup count: " << fifoCount << std::endl; }
    }
    // Read FIFO count
    while (iteration < numofelements) {
        mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
        fifoCount = MergeHL(data.at(0), data.at(1));
        if (DebugMode) { std::cout << "ReadDummyFIFO count: " << fifoCount << std::endl; }
        if (fifoCount >= cfg.FIFO_BURST_SIZE * 2) {
            std::vector<uint8_t> fifoData(cfg.FIFO_BURST_SIZE);
            auto readstart = std::chrono::high_resolution_clock::now();
            mcp.i2cRead(address, ICM20948_FIFO_RW, fifoData);
            iteration += cfg.FIFO_PACKET_MULT;
        }
        if (DebugMode) {
            std::cout << "Dummy fifo read, size: " << fifoCount << ", iteration: " << iteration << std::endl;
        }
    }
    return true;
}


bool ICM20948::ReadFIFO() const {
    std::cout << "Press 'S' to start measuring" << std::endl;
    while (true) {
        if (GetAsyncKeyState('S') & 0x8000) {
            break;
        }
        mcp.wait(50);
    }
    SelectBank(0);
    FlushFIFO(250, 200);
    std::cout << "Reading FIFO" << std::endl;

    std::vector<float> sumAccel = {0.f, 0.f, 0.f};
    std::vector<float> sumGyro = {0.f, 0.f, 0.f};
    std::vector<uint8_t> data(2);
    uint16_t fifoCount = 0;
    float AccelSquare = 0.0f;
    bool offsetActive = false;
    bool oPressedLast = false;
    bool oPressedJustNow = false;
    bool LPressedLast = false;
    bool LPressedJustNow = false;
    bool LogActive = false;
    std::ofstream LogFile; // global file stream


    std::array<float,3>EulerOffset_; //RADIANS!

    int underflowCount = 0;
    int overflowCount = 0;
    //FIFO
    uint8_t _fifoPacketSize = 12;
    uint8_t _fifoPacketMult = 10;
    uint16_t _fifoBurstSize = _fifoPacketSize * _fifoPacketMult;
    uint16_t _fifoCountThres = _fifoBurstSize * 3;

    float roll_deg = 0.0f, pitch_deg = 0.0f, yaw_deg = 0.0f;

    bool fifoStopped = false;


    while (fifoCount < _fifoCountThres) {
        // Read FIFO count
        mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
        fifoCount = MergeHL(data.at(0), data.at(1));
    }
    //LOGGING
    // std::string basefilename = "SensorLog_";
    // auto currtime = std::format("{:%F_%H-%M}", std::chrono::zoned_time{
    //                                 "Europe/Budapest",
    //                                 std::chrono::system_clock::now()
    //                             });
    // std::ofstream LogFile("../../Log/" + basefilename + currtime + ".txt");
    // // std::cout << "FIleName: " << "../../" << basefilename << currtime << ".txt" << std::endl;
    // LogFile << "ITERATION;GYRO_X;GYRO_Y;GYRO_Z;ACCEL_X;ACCEL_Y;ACCEL_Z;ROLL;PITCH;YAW;TIMESTAMP" << std::endl;

    int iterator = 0;
    auto loopstart = std::chrono::high_resolution_clock::now();
    while (true) {
        auto start = std::chrono::high_resolution_clock::now();
        // Check for user abort
        if (GetAsyncKeyState('C') & 0x8000) {
            // Windows specific
            std::cout << "Exit on 'C' pressed." << std::endl;
            break;
        }
        bool oPressed = (GetAsyncKeyState('O') & 0x8000);
        if (oPressed && !oPressedLast) {
            offsetActive = !offsetActive; // toggle or capture offset
            oPressedJustNow = offsetActive;
            std::cout << "OffsetActive: " << offsetActive << ", opressedjustnow: " << oPressedJustNow << std::endl;
        }
        oPressedLast = oPressed;

        bool LPressed = (GetAsyncKeyState('L') & 0x8000);
        if (LPressed && !LPressedLast) {
            if (!LogActive) {
                //LOGGING
                std::string basefilename = "SensorLog_";
                auto currtime = std::format("{:%F_%H-%M-%S}", std::chrono::zoned_time{
                                                "Europe/Budapest",
                                                std::chrono::system_clock::now()
                                            });
                int pos = currtime.rfind(".");
                currtime.erase(pos);
                LogFile.open("../../Log/" + basefilename + currtime + ".txt");

                LogFile << "ITERATION;GYRO_X;GYRO_Y;GYRO_Z;ACCEL_X;ACCEL_Y;ACCEL_Z;ROLL;PITCH;YAW;TIMESTAMP" <<
                        std::endl;
                std::cout << "Logging started: " << "../../" << basefilename << currtime << ".txt" << std::endl;
            } else {
                LogFile.close();
                std::cout << "Logging finished" << std::endl;
            }
            LogActive = !LogActive;
        }
        LPressedLast = LPressed;

        // Read FIFO count
        mcp.i2cRead(address, ICM20948_FIFO_COUNTH, data);
        fifoCount = MergeHL(data.at(0), data.at(1));

        if (fifoCount <= _fifoBurstSize) {
            std::cout << "FIFO underflow, (" << fifoCount << "), underflowcount: " << underflowCount << std::endl;
            if (++underflowCount >= 30) {
                std::cout << "FIFO empty" << std::endl;
                break;
            }
            continue;
        }

        if (fifoCount >= cfg.FIFO_MAX_SIZE - _fifoCountThres) {
            std::cout << "FIFO overflow(" << fifoCount << "), overflowcount: " << overflowCount << std::endl;
            if (++overflowCount >= 30) {
                std::cout << "FIFO overflow" << std::endl;
                break;
            }
        }

        // Adjust sample rate if needed
        if (fifoCount <= _fifoBurstSize * 2) {
            //mcp.wait(1500);
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Underflow" << std::endl;
            continue;
        }
        if (fifoCount >= _fifoBurstSize * 10) {
            // std::vector<uint8_t> fifoendata = {ICM20948_FIFO_EN_2, 0x00};
            // mcp.i2cWrite(address, fifoendata);
            _fifoPacketMult = 15;
            _fifoBurstSize = _fifoPacketSize * _fifoPacketMult;
            _fifoCountThres = _fifoBurstSize * 3;
            fifoStopped = true;
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "Overflow, fifo count: " << fifoCount << std::endl;
        }
        if (fifoStopped) {
            if (fifoCount <= _fifoBurstSize * 2.5) {
                //Enable Accel, Gyro_X_Y_Z but disable LSB (TEMP). MAybe enable for temp compensation TODO!
                // std::vector<uint8_t> fifoendata = {ICM20948_FIFO_EN_2, 0b00011110};
                // mcp.i2cWrite(address, fifoendata);
                _fifoPacketMult = 10;
                _fifoBurstSize = _fifoPacketSize * _fifoPacketMult;
                _fifoCountThres = _fifoBurstSize * 3;
                auto start = std::chrono::high_resolution_clock::now();
                std::cout << "FIFO Enabled" << std::endl;
                fifoStopped = false;
            }
        }

        if (fifoCount >= _fifoBurstSize) {
            std::vector<uint8_t> fifoData(_fifoBurstSize);
            auto readstart = std::chrono::high_resolution_clock::now();
            mcp.i2cRead(address, ICM20948_FIFO_RW, fifoData);


            // Parse packets
            for (int p = 0; p < _fifoPacketMult; p++) {
                iterator++;
                std::vector<float> AccelGyroData(7); // [0:2] accel, [3:5] gyro
                for (int i = 0; i < 6; i++) {
                    int idx = p * _fifoPacketSize + i * 2;
                    float rawData = MergeHL(fifoData[idx], fifoData[idx + 1]);
                    if (i < 3) {
                        //AccelGyroData.at(i) = (rawData * accelconfig.scale) - accelbias.at(i); // g
                        AccelGyroData.at(i) = (rawData * accelconfig.scale); // gs
                        sumAccel.at(i) += AccelGyroData.at(i);
                        AccelSquare += AccelGyroData.at(i) * AccelGyroData.at(i);
                    } else {
                        //AccelGyroData.at(i) = (rawData * gyroconfig.scale) - gyrobias.at(i - 3); // dps
                        AccelGyroData.at(i) = (rawData * gyroconfig.scale) - gyrobias_stored.at(i - 3); // dps
                        sumGyro.at(i - 3) += AccelGyroData.at(i);
                    }
                }
                //TIME
                auto readend = std::chrono::high_resolution_clock::now();
                long long elapsed_ms;
                if (p == 0) {
                    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(readend - start).count();
                } else {
                    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(readend - readstart).count();
                }
                float sensor_delay = std::round(fifoCount / _fifoBurstSize) * elapsed_ms; // average dt for each sample
                AccelGyroData.at(6) = sensor_delay;

                //Normalise accel
                float AccelSquared = sqrtf(AccelSquare);
                if (AccelSquared > 1e-1f) {
                    AccelGyroData.at(0) /= AccelSquared;
                    AccelGyroData.at(1) /= AccelSquared;
                    AccelGyroData.at(2) /= AccelSquared;
                } else { std::cerr << "Acceleration normalization error" << std::endl; }

                //Normal use
                MadgwickAHRSupdateIMU(AccelGyroData[3] * DEG2RAD, AccelGyroData[4] * DEG2RAD,
                                      AccelGyroData[5] * DEG2RAD,
                                      AccelGyroData[0], AccelGyroData[1], AccelGyroData[2]);
                //we want to cancel out gimbal lock on IMU pitch/Y axis to mount it perpendicular so we swap x and y, (maybe -1*z?)
                // MadgwickAHRSupdateIMU(AccelGyroData[4] * DEG2RAD, AccelGyroData[3] * DEG2RAD,
                //                      -AccelGyroData[5] * DEG2RAD,
                //                      AccelGyroData[1], AccelGyroData[0], -AccelGyroData[2], freq_per_sample);


                AccelSquare = 0.0f;
                std::array<float,3> euler_;
                QuaternionsToEulerAngles(euler_);
                //eulerAngles(euler);
                //eulerAnglesRPswap(euler);
                // Convert to degrees
                if (oPressedJustNow) {
                    EulerOffset_.at(0) = euler_.at(0);
                    EulerOffset_.at(1) = euler_.at(1);
                    EulerOffset_.at(2) = euler_.at(2);
                    oPressedJustNow = false;
                }
                if (offsetActive) {
                    roll_deg = (euler_[0] - EulerOffset[0]) * RAD2DEG;
                    pitch_deg = (euler_[1] - EulerOffset[1]) * RAD2DEG;
                    yaw_deg = (euler_.at(2) - EulerOffset[2]) * RAD2DEG;
                } else {
                    roll_deg = euler_.at(0) * RAD2DEG;
                    pitch_deg = euler_.at(1) * RAD2DEG;
                    yaw_deg = euler_.at(2) * RAD2DEG;
                }
                if (LogActive) {
                    LogFile << iterator << ";"
                            << AccelGyroData[3] << ";" << AccelGyroData[4] << ";" << AccelGyroData[5] << ";" // gyro
                            << AccelGyroData[0] << ";" << AccelGyroData[1] << ";" << AccelGyroData[2] << ";" // accel
                            << roll_deg << ";" << pitch_deg << ";" << yaw_deg << ";" //madgwick output
                            << AccelGyroData[6] << std::endl; //timestamp
                }
            }
        }


        std::cout << "FIFO size: " << fifoCount << ", Roll: " << std::setprecision(2) << roll_deg
                << ", Pitch: " << std::setprecision(2) << pitch_deg
                << ", Yaw: " << std::setprecision(2) << yaw_deg
                << std::endl;
    }
    if (LogActive) { LogFile.close(); }

    auto loopend = std::chrono::high_resolution_clock::now();
    auto loopduration = std::chrono::duration_cast<std::chrono::milliseconds>(loopend - loopstart);
    std::cout << "Loop duration: " << loopduration.count() << " ms" << std::endl;
    return true;
}

std::vector<float> ICM20948::ReadGyroOffsets() const {
    SelectBank(2);
    mcp.wait(50);
    std::vector<float> offsets(3);
    std::vector<uint8_t> data(6);
    mcp.i2cRead(address, ICM20948_XG_OFFS_H, data);
    std::cout << "Internal gyro offsets: ";
    for (int i = 0; i < offsets.size(); ++i) {
        int16_t rawData = MergeHL(data.at(i * 2), data.at(i * 2 + 1));
        offsets.at(i) = static_cast<float>(rawData) * ICM20948_GYRO_OFFS_STPSIZE;
        std::cout << offsets.at(i) << " ";
    }
    std::cout << std::endl;
    int err = mcp.GetLastError();
    if (err < 0) { std::cerr << "Error getting gyro offsets, " << err << std::endl; }
    return offsets;
}

void ICM20948::WriteGyroOffsets(const std::vector<float> &gyroOffsets) const {
    SelectBank(2);
    mcp.wait(100);
    uint8_t XG_OFFS_STARTREG = ICM20948_XG_OFFS_H;
    std::vector<uint8_t> data(6);
    for (int i = 0; i < gyroOffsets.size(); i++) {
        int16_t rawData = std::lround(gyroOffsets.at(i) / ICM20948_GYRO_OFFS_STPSIZE);
        data.at(2 * i) = static_cast<uint8_t>((rawData >> 8) & 0b11111111);
        data.at(2 * i + 1) = static_cast<uint8_t>(rawData & 0b11111111);
    }
    for (int i = 0; i < data.size(); i++) {
        std::vector<uint8_t> temp = {static_cast<uint8_t>(XG_OFFS_STARTREG + i), data.at(i)};
        mcp.i2cWrite(address, temp);
        // std::cout << (int)temp.at(0) << " " << (int)temp.at(1) <<std::endl;
        mcp.wait(50);
    }
    int err = mcp.GetLastError();
    if (err < 0) { std::cerr << "Error writing gyro offsets, " << err << std::endl; }
}


void ICM20948::ReadExtSlvReg(int size) const {
    std::cout << "Calibrate mag, press M to end";
    //LOGGING
    std::string filename = "MagLog.txt";

    auto currtime = std::chrono::system_clock::now();
    filename += currtime.time_since_epoch().count();
    std::ofstream MagLog("../../" + filename);
    MagLog << "ITERATION;MAG_X;MAG_Y;MAG_Z;TIMESTAMP" << std::endl;
    int iteration = 1;
    std::vector<uint8_t> data(size);
    int16_t _hx, _hy, _hz;
    mcp.wait(500);
    SelectBank(0);
    while (true) {
        if (GetAsyncKeyState('M') & 0x8000) {
            // Windows specific
            std::cout << "Exit on 'M' pressed." << std::endl;
            break;
        }
        auto start = std::chrono::high_resolution_clock::now();
        mcp.i2cRead(address, 0x3B, data);
        _hx = MergeHL(data.at(1), data.at(0));
        _hy = MergeHL(data.at(3), data.at(2));
        _hz = MergeHL(data.at(5), data.at(4));
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        MagLog << iteration << ";" << _hx << ";" << _hy << ";" << _hz << ";" << elapsed.count() << std::endl;
        std::cout << iteration << ";" << _hx << ";" << _hy << ";" << _hz << ";" << elapsed.count() << std::endl;
        iteration++;
    }
    MagLog.close();
}
