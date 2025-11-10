#pragma once

#include <QString>

namespace exo::redis::keys {

    // Hash keys
    inline constexpr auto CONF_ENV = "conf:env";
    inline constexpr auto RUN_ADDRS = "run:addrs";
    inline constexpr auto LOG_STREAM = "log";

    // Stream key prefix
    inline constexpr auto XDATA_PREFIX = "xdata:";

    // Command key prefix
    inline constexpr auto COMMAND_PREFIX = "command:";

    // Telemetry field names
    inline constexpr auto FIELD_ENABLED = "enabled";
    inline constexpr auto FIELD_SLOT_INDEX = "slot_idx";
    inline constexpr auto FIELD_CMD_COUNTER = "cmd_cntr";
    inline constexpr auto FIELD_POSITION = "position";
    inline constexpr auto FIELD_TORQUE = "torque";
    inline constexpr auto FIELD_TIMESTAMP = "t";
    inline constexpr auto FIELD_RETRY_COUNT = "n_tries";

    // Environment hash field names
    inline constexpr auto FIELD_CONTROLLER_CMD = "controller_cmd";
    inline constexpr auto FIELD_MOTOR_E_FLEX = "motor_e_flex";
    inline constexpr auto FIELD_MOTOR_E_EXT = "motor_e_ext";
    inline constexpr auto FIELD_MOTOR_S_FLEX = "motor_s_flex";
    inline constexpr auto FIELD_MOTOR_S_EXT = "motor_s_ext";
    inline constexpr auto FIELD_MOTOR_S_ADD_PRON = "motor_s_add_pron";
    inline constexpr auto FIELD_MOTOR_S_ADD_SUP = "motor_s_add_sup";
    inline constexpr auto FIELD_MOTOR_S_ABD = "motor_s_abd";
    inline constexpr auto FIELD_MOCK_MOTORS = "mock_motors";
    inline constexpr auto FIELD_MULTIPORT_MOTORS = "multiport_motors";
    inline constexpr auto FIELD_RESTAPI_PORT = "restapi_port";
    inline constexpr auto FIELD_DB_ADMIN_PW = "db_admin_pw";
    inline constexpr auto FIELD_LOG_FOLDER = "log_folder";
    inline constexpr auto FIELD_SCRIPTS_FOLDER = "scripts_folder";
    inline constexpr auto FIELD_FUNCTIONS_FOLDER = "functions_folder";
    inline constexpr auto FIELD_TASK_FOLDER = "task_folder";
    inline constexpr auto FIELD_TASKS = "tasks";

    inline QString xdataKeyForAddress(int addr)
    {
        return QString::fromLatin1(XDATA_PREFIX) + QString::number(addr);
    }

    inline QString commandKeyForAddress(int addr)
    {
        return QString::fromLatin1(COMMAND_PREFIX) + QString::number(addr);
    }

} // namespace exo::redis::keys
