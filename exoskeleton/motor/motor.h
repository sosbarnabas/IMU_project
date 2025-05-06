#pragma once

#include <cstdint>
#include <iostream>
#include <optional>
#include <tuple>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <vector>
#include <string>

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QtEndian>
#include <QDebug>

namespace exoskeleton::motor {

constexpr uint8_t HEADER = 0x0A;
constexpr uint8_t ADDR = 0x00;

constexpr uint8_t CMD_ENABLE = 0x01;
constexpr uint8_t CMD_DISABLE = 0x10;
constexpr uint8_t CMD_SET_ZERO = 0x02;
constexpr uint8_t CMD_SET_OFFSET = 0x20;
constexpr uint8_t CMD_SET_FUNC_AT_SLOT = 0x30;
constexpr uint8_t CMD_SELECT_SLOT = 0x04;

constexpr int32_t FULL_TURN = 32768;
constexpr int8_t TORQUE_MIN = -127;
constexpr int8_t TORQUE_MAX = 127;
constexpr size_t FUNCTION_LEN = 360;

extern bool log_command;

using SingleMotorDataTuple = std::tuple<bool, int, int, int32_t, int8_t>;

struct SingleMotorData {
    bool enabled;
    int32_t slot_idx;
    int32_t cmd_cntr;
    int32_t position;
    int32_t torque;

    SingleMotorData(bool en, int32_t slot, int32_t cmd, int32_t pos, int32_t tq);
    SingleMotorDataTuple to_tuple() const;

    static SingleMotorData empty() {
        return SingleMotorData{false, 0, 0, 0, 0};
    }

    const SingleMotorData& value() const {
        return *this;
    }

    bool has_value() const {
        return enabled ||
               slot_idx != 0 ||
               cmd_cntr != 0 ||
               position != 0 ||
               torque != 0;
    }

    friend std::ostream& operator<<(std::ostream &os, const SingleMotorData& data);
};

std::string find_cstny_usb_com_port();
QSerialPort* open_serial(int baudrate);

int8_t calculateChecksum(const QByteArray &data);
SingleMotorData read_data(QSerialPort* serial, int max_tries);

void send(QSerialPort* ser, int command, const QByteArray& data, int addr);
void motor_set_zero(QSerialPort* ser, int addr);
void motor_enable(QSerialPort* ser, int addr );
void motor_disable(QSerialPort* ser, int addr);
void motor_set_offset(QSerialPort* ser, int value, int addr);
void motor_select_slot(QSerialPort* ser, int value);

void motor_set_slot_function(QSerialPort* ser, int slot, const std::vector<int>& function_input);

void reader_daemon(QSerialPort* ser);

}
