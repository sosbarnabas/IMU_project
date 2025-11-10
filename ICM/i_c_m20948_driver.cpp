// icm20948.cpp
#include "icm20948.h"
// Ide kell a valódi MCP2221 driver header-je
// #include "mcp2221.hpp" // példa

#include <cstring>
#include <algorithm>
#include <cmath>

ICM20948::ICM20948(MCP2221 &mcpRef, uint8_t addr, int imu_id): mcp(mcpRef), address(addr),
                                                   gyroconfig(GYRO_HIGH), accelconfig(ACCEL_HIGH) {
}

ICM20948::~ICM20948() {
    stop();
}

bool ICM20948::Initialize() {
    // Példa: WHOAMI olvasás, reset, órajelek, enable szenzor blokkok stb.
    // A tényleges regisztercímek/értékek a meglévő kódodból tölthetők be.
    // select_bank_(0); read_regs_(WHOAMI, &val, 1) ...
    return true;
}

bool ICM20948::FIFOConfig() {
    // FIFO konfigurálása (források, packet size, watermark stb.)
    return true;
}

bool ICM20948::CalibrateAccelGyro(uint16_t N) {
    // Gyors skeleton: olvas N mintát, átlagol, eltárol bias-ba
    // Itt csak helyőrző:
    gyro_bias_session_ = {0.f, 0.f, 0.f};
    return true;
}

void ICM20948::start(ConcurrentQueue<ImuSample>& out, const ImuConfig& cfg) {
    stop(); // biztos ami biztos, ne legyen dupla szál
    running_ = true;
    worker_ = std::thread(&ICM20948::producer_loop_, this, &out, cfg);
}

void ICM20948::stop() {
    if (running_.exchange(false)) {
        if (worker_.joinable()) worker_.join();
    }
}

void ICM20948::set_zero_euler(const float euler0[3]) {
    std::copy(euler0, euler0+3, euler_zero_.begin());
}

void ICM20948::set_use_bias(bool on) { use_bias_ = on; }
void ICM20948::set_use_stored_bias(bool stored) { use_stored_bias_ = stored; }
void ICM20948::set_gyro_bias_stored(const float b[3]) {
    std::copy(b, b+3, gyro_bias_stored_.begin());
}
void ICM20948::set_gyro_bias_session(const float b[3]) {
    std::copy(b, b+3, gyro_bias_session_.begin());
}

// Low-level helpers – ezekbe helyezd át az MCP I2C hívásokat
bool ICM20948::select_bank_(uint8_t bank) const {
    // return write_reg_(REG_BANK_SEL, bank << 4);
    return true;
}
bool ICM20948::write_reg_(uint8_t reg, uint8_t val) const {
    // return mcp_.i2cWrite(addr_, reg, &val, 1);
    return true;
}
bool ICM20948::read_regs_(uint8_t reg, uint8_t* buf, size_t n) const {
    // return mcp_.i2cRead(addr_, reg, buf, n);
    return true;
}

bool ICM20948::read_fifo_count_(uint16_t& count) const {
    // Olvasd ki a FIFO COUNT regiszter(eke)t -> count
    count = 0;
    return true;
}

bool ICM20948::read_fifo_burst_(std::vector<uint8_t>& buf, size_t n) const {
    buf.resize(n);
    // I2C burst read a FIFO-ból
    return true;
}

void ICM20948::parse_fifo_packet_(const uint8_t* pkt, const ImuConfig& cfg, ImuSample& s) const {
    // A tényleges packet layout szerint:
    // int16_t ax=..., ay=..., az=..., gx=..., gy=..., gz=...
    // s.accel[i] = ax*cfg.ranges.accel_scale; stb.
    // Bias alkalmazás, ha use_bias_ = true
    if (use_bias_) {
        const auto& b = use_stored_bias_ ? gyro_bias_stored_ : gyro_bias_session_;
        s.gyro[0] -= b[0];
        s.gyro[1] -= b[1];
        s.gyro[2] -= b[2];
    }
}

void ICM20948::fuse_imu_only_(const ImuSample& s, float euler_deg[3]) {
    // Gyors előnézeti fúzió (pl. komplementer szűrő, itt helyőrző)
    euler_deg[0] = 0; euler_deg[1] = 0; euler_deg[2] = 0;
}

void ICM20948::producer_loop_(ConcurrentQueue<ImuSample>* out, ImuConfig cfg) {
    const auto t0 = std::chrono::steady_clock::now();

    // FIFO / ODR beállítás javasolt itt vagy a hívóban (Initialize+FIFOConfig után)
    uint32_t seq_local = 0;

    while (running_) {
        uint16_t fifo_count_bytes = 0;
        if (!read_fifo_count_(fifo_count_bytes)) {
            ++i2c_errors_;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const int packets_available = cfg.fifo_packet_size_bytes > 0
            ? fifo_count_bytes / cfg.fifo_packet_size_bytes
            : 0;

        if (packets_available < cfg.fifo_underflow_threshold_packets) {
            // kevés adat – várunk kicsit
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        int burst_packets = std::min(
            packets_available,
            cfg.fifo_packet_mult_burst * std::max(1, cfg.fifo_underflow_threshold_packets)
        );
        size_t burst_bytes = static_cast<size_t>(burst_packets) * cfg.fifo_packet_size_bytes;

        std::vector<uint8_t> raw;
        if (!read_fifo_burst_(raw, burst_bytes)) {
            ++i2c_errors_;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const uint8_t* p = raw.data();
        for (int i = 0; i < burst_packets; ++i, p += cfg.fifo_packet_size_bytes) {
            ImuSample s{};
            s.imu_id = imu_id_;
            s.seq = ++seq_local;
            auto now = std::chrono::steady_clock::now();
            s.t_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - t0).count();

            parse_fifo_packet_(p, cfg, s);

            if (cfg.enable_mag) {
                // töltés s.mag-be, ha a packet tartalmazza
            }

            // opcionális preview
            float euler_deg[3];
            fuse_imu_only_(s, euler_deg);
            s.euler = std::array<float,3>{euler_deg[0] - euler_zero_[0],
                                          euler_deg[1] - euler_zero_[1],
                                          euler_deg[2] - euler_zero_[2]};

            out->push(std::move(s));
        }

        if (packets_available > cfg.fifo_overflow_threshold_packets) {
            ++fifo_overflows_;
        }
    }
}
