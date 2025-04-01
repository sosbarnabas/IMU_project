#include <iostream>
#include <optional>
#include <tuple>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <QSerialPort>
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QDebug>
#include <QtEndian>
namespace exoskeleton::motor {
    // Constants
    constexpr uint8_t HEADER = 0x0A;
    constexpr uint8_t ADDR = 0x00;  // address of module #1

    constexpr uint8_t CMD_ENABLE = 0x01;
    constexpr uint8_t CMD_DISABLE = 0x10;
    constexpr uint8_t CMD_SET_ZERO = 0x02;
    constexpr uint8_t CMD_SET_OFFSET = 0x20;
    // constexpr uint8_t CMD_SET_FUNCTION = 0x03;
    constexpr uint8_t CMD_SET_FUNC_AT_SLOT = 0x30;
    constexpr uint8_t CMD_SELECT_SLOT = 0x04;

    constexpr int32_t FULL_TURN = 32768;
    constexpr int8_t TORQUE_MIN = -127;
    constexpr int8_t TORQUE_MAX = 127;
    constexpr size_t FUNCTION_LEN = 360;

    bool log_command = false;

    using SingleMotorDataTuple = std::tuple<bool, int, int, int32_t, int8_t>;

    struct SingleMotorData {
        bool enabled;
        int32_t slot_idx;
        int32_t cmd_cntr;
        int32_t position;
        int32_t torque;

        SingleMotorData(bool en, int32_t slot, int32_t cmd, int32_t pos, int32_t tq)
            : enabled(en), slot_idx(slot), cmd_cntr(cmd), position(pos), torque(tq) {}

        SingleMotorDataTuple to_tuple() const {
            return std::make_tuple(enabled, slot_idx, cmd_cntr, position, torque);
        }

        friend std::ostream& operator<<(std::ostream &os, const SingleMotorData& data);
    };

    std::ostream& operator<<(std::ostream& os, const SingleMotorData& data){
        os << "enable: " << data.enabled << " slot_idx: " << data.slot_idx << " cmd_cntr: " << data.cmd_cntr << " pos: " << data.position << " torq: " << data.torque;
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

    QSerialPort* open_serial(int baudrate = 1000000) {
        std::string usb_com_port = find_cstny_usb_com_port();
        if (usb_com_port.empty()) {
            throw std::runtime_error("Cannot find COM port");
        }

        QSerialPort* ser = new QSerialPort(QString::fromStdString(usb_com_port));
        ser->setBaudRate(baudrate);

        if (!ser->open(QIODevice::ReadWrite)) {
            throw std::runtime_error("Failed to open serial port");
        }
        
        ser->setBaudRate(baudrate);
        ser->setDataBits(QSerialPort::Data8);
        ser->setParity(QSerialPort::NoParity);
        ser->setStopBits(QSerialPort::OneStop);
        
        return ser;
    }

    uint8_t calculateChecksum(const QByteArray &data) {
        if (data.size() < 1) {
            return 0;
        }
        auto sum = int{0};
        for (auto const e : data) {
            sum += int{e};
        }
        return sum & 0xFF;//std::accumulate(data.begin(), data.end() - 1, int{0}) & 0xFF;
    }

    SingleMotorData read_data(QSerialPort* serial, int max_tries = 10) {
        serial->waitForReadyRead(100);
        QByteArray buffer;

        int tries = 0;
        while (tries < max_tries) {
            if (serial->waitForReadyRead(100)) {
                buffer += serial->readAll();

                while (buffer.size() >= 8) { // HEADER(1) + DATA(7)
                    int headerIndex = buffer.indexOf(HEADER);
                    if (headerIndex < 0) {
                        buffer.clear(); // nincs HEADER, buffer törlése
                        break;
                    }
                    if (buffer.size() < headerIndex + 8) {
                        break; // még nem érkezett meg a teljes csomag
                    }

                    QByteArray data_with_header = buffer.mid(headerIndex, 8);  // HEADER + 7 byte adat

                    buffer.remove(0, headerIndex + 8); // feldolgozott adatok törlése

                    if (calculateChecksum(data_with_header) == static_cast<uint8_t>(data_with_header.at(7))) {
                        QByteArray data = data_with_header.mid(1, 6); // Csak adat checksum nélkül

                    if (true) { //calculated_checksum == received_checksum
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
                        qWarning() << "Checksum hiba, adatok dobása...";
                    }

                }
            }
            tries++;
        }

        throw std::runtime_error("Failed to read data");
    }


    void send(QSerialPort* ser, int command, const QByteArray& data = {}, int addr = ADDR) {
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
        qint64 bytesWritten = ser->write(full_command);
        if (bytesWritten != full_command.size()) {
            qWarning() << "Nem sikerült az összes bájt elküldése";
        }
    }

    void motor_set_zero(QSerialPort* ser, int addr = ADDR) {
        send(ser, CMD_SET_ZERO, {}, addr);
    }

    void motor_enable(QSerialPort* ser, int addr = ADDR) {
        send(ser, CMD_ENABLE, {}, addr);
    }

    void motor_disable(QSerialPort* ser, int addr = ADDR) {
        send(ser, CMD_DISABLE, {}, addr);
    }

    void motor_set_offset(QSerialPort* ser, int value, int addr = ADDR) {
        QByteArray offset;
        QDataStream stream(&offset, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<qint32>(value);

        send(ser, CMD_SET_OFFSET, offset, addr);
    }

    void motor_select_slot(QSerialPort* ser, int value) {
        QByteArray slot_no;
        QDataStream stream(&slot_no, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<qint8>(value);
        send(ser, CMD_SELECT_SLOT, slot_no);
    }

    void motor_set_slot_function(QSerialPort* ser, int slot, const std::vector<int>& function_input) {
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

    void reader_daemon(QSerialPort *ser) {
        while (true) {
            auto data = read_data(ser);
            std::cout << data<< std::endl;
        }
    }
}