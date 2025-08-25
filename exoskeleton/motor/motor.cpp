#include "motor.h"

#include <iostream>
#include <optional>
#include <tuple>
#include <stdexcept>
#include <algorithm>
#include <QSerialPort>
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QDebug>
#include <QtEndian>
#include <thread>
namespace exoskeleton::motor {

    namespace { // internal
        [[nodiscard]] auto calculateChecksum(const QByteArray &data) -> int8_t {
            if (data.size() < 1) {
                return 0;
            }
            auto sum = int{0};
            for (auto const e : data) {
                sum += int{e};
            }
            return sum & 0xFF;
        }

        auto send(QSerialPort& ser, const int command, const QByteArray& data = {}, const int addr = ADDR) -> void {
            QByteArray full_command;
            full_command.append(static_cast<uint8_t>(HEADER));
            full_command.append(static_cast<uint8_t>(addr));
            full_command.append(static_cast<uint8_t>(command));
            if (data.size() > 0) {
                full_command.append(data);
            }
            uint8_t checksum = calculateChecksum(full_command);
            full_command.append(checksum);
            // qDebug() << full_command.toStdString() << '\n';
            qint64 bytesWritten = ser.write(full_command);
            if (bytesWritten != full_command.size()) {
                qWarning() << "Nem sikerült az összes bájt elküldése";
            }
        }
    }

    SingleMotorData::SingleMotorData(bool en, int32_t slot, int32_t cmd, int32_t pos, int32_t tq)
            : enabled(en), slot_idx(slot), cmd_cntr(cmd), position(pos), torque(tq) {}

    auto SingleMotorData::is_valid() const -> bool {
        return !(slot_idx == 0 && cmd_cntr == 0 && position == 0 && torque == 0 && !enabled);
    }

    auto SingleMotorData::value() const -> SingleMotorData const& {
        return *this;
    }

    auto SingleMotorData::to_tuple() const -> SingleMotorDataTuple {
        return std::make_tuple(enabled, slot_idx, cmd_cntr, position, torque);
    }

    auto SingleMotorData::empty() -> SingleMotorData {
        return SingleMotorData{false, 0, 0, 0, 0};
    }

    auto SingleMotorData::has_value() const -> bool {
        return enabled ||
               slot_idx != 0 ||
               cmd_cntr != 0 ||
               position != 0 ||
               torque != 0;
    }

    std::ostream& operator<<(std::ostream& os, const SingleMotorData& data){
        os << "enabled: " << data.enabled << " slot_idx: " << data.slot_idx << " cmd_cntr: " << data.cmd_cntr << " pos: " << data.position << " torq: " << data.torque;
        return os;
    }

    std::string find_cstny_usb_com_port() {
        for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
            if (!port.serialNumber().isEmpty() && port.serialNumber().startsWith("CSTNY")) {
                return port.portName().toStdString();
            }
        }
        return "";
    }

    auto find_port_name_by_serial_num(std::string const& sn) -> std::string {
        for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
            if (port.serialNumber() == sn) {
                return port.portName().toStdString();
            }
        }
        throw SerialNumberNotFound{sn};
    }

    auto open_serial_port(std::string const& name) -> std::unique_ptr<QSerialPort> {
        auto ser = std::make_unique<QSerialPort>(QString::fromStdString(name));

        if (!ser->open(QIODevice::ReadWrite)) {
            throw CannotOpenSerialPort{name};
        }

        ser->setBaudRate(1'000'000);
        ser->setDataBits(QSerialPort::Data8);
        ser->setParity(QSerialPort::NoParity);
        ser->setStopBits(QSerialPort::OneStop);

        return ser;
    }

    SingleMotorData read_data(QSerialPort& serial, int max_tries) {
        //serial->waitForReadyRead(100);

        QByteArray buffer;
        serial.flush();
        int tries = 0;
        while (tries < max_tries) {
            if (serial.waitForReadyRead(100)) {
                buffer += serial.readAll();
                while (buffer.size() >= 8) { // HEADER(1) + DATA(7)
                    int headerIndex = buffer.indexOf(HEADER);
                    if (headerIndex < 0) {
                        buffer.clear(); // nincs HEADER, buffer törlése
                        break;
                    }
                    if (buffer.size() < headerIndex + 8) {
                        break; // még nem érkezett meg a teljes csomag
                    }

                    QByteArray data = buffer.mid(headerIndex + 1, 7);
                    auto checksumData = buffer.mid(headerIndex, 7);
                    buffer.remove(0, headerIndex + 8); // feldolgozott adatok törlése

                    auto calculated_checksum = calculateChecksum(checksumData);
                    auto received_checksum = data.at (data.size()-1);
                    if (calculated_checksum == received_checksum) {
                        int32_t first = static_cast<int32_t>(data.at(0));
                        bool enabled = (first >> 7) & 0b1;
                        int slot_idx = (first >> 4) & 0b111;
                        int cmd_cntr = first & 0b1111;

                        int32_t position = qFromBigEndian<int32_t>(
                            reinterpret_cast<const uchar*>(data.constData() + 1)
                        );
                        int32_t torque = static_cast<int32_t>(data.at(5));
                        return SingleMotorData(enabled, slot_idx, cmd_cntr, position, torque);
                    } else {
                        std::cerr << "Checksum hiba, adatok dobása..." << std::endl;
                    }
                }
            }
            tries++;
        }

        throw std::runtime_error("Failed to read data");
    }




    void motor_set_zero(QSerialPort& ser, int addr) {
        send(ser, CMD_SET_ZERO, {}, addr);
    }

    void motor_enable(QSerialPort& ser, int addr) {
        send(ser, CMD_ENABLE, {}, addr);
    }

    void motor_disable(QSerialPort& ser, int addr) {
        send(ser, CMD_DISABLE, {}, addr);
    }

    void motor_set_offset(QSerialPort& ser, int value, int addr) {
        QByteArray offset;
        QDataStream stream(&offset, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<qint32>(value);

        send(ser, CMD_SET_OFFSET, offset, addr);
    }

    void motor_select_slot(QSerialPort& ser, int value) {
        QByteArray slot_no;
        QDataStream stream(&slot_no, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<qint8>(value);
        send(ser, CMD_SELECT_SLOT, slot_no);
    }

    void motor_set_slot_function(QSerialPort& ser, int slot, const std::vector<int>& function_input) {
        QByteArray data;

        // Slot number - 1 signed byte
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<qint8>(slot);

        // Biztosítjuk, hogy pontosan 360 érték legyen, értékeik [-127, 127] közé legyenek normalizálva
        for (int i = 0; i < 360; ++i) {
            int val = (i < static_cast<int>(function_input.size())) ? function_input[i] : 0;
            val = std::clamp(val, -127, 127);
            data.append(static_cast<char>(static_cast<qint8>(val)));
        }

        send(ser, CMD_SET_FUNC_AT_SLOT, data);
    }

} // exoskeleton::motor