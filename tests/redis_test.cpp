#include <iostream>
#include "../exoskeleton/redis/RedisFacade.h"
#include "../exoskeleton/redis/RedisTools.h"
#include "../exoskeleton/settings/Settings.h"
#include "../exoskeleton/settings/UserParams.h"
#include <unordered_map>

using namespace exoskeleton::redis;
using namespace exoskeleton::settings;

int main() {
    std::unique_ptr<Facade> facade = std::make_unique<Facade>("redis://localhost:6379");

    // Clear test data
    facade->_redis.flushall();

    // Setup test environment settings
    std::unordered_map<std::string, std::string> env_settings = {
        {"motor_e_flex", "motor1"},
        {"motor_e_ext", "motor2"},
        {"motor_s_flex", "motor3"},
        {"motor_s_ext", "motor4"},
        {"motor_s_add_pron", "motor5"},
        {"motor_s_abd", "motor6"},
        {"motor_s_add_sup", "motor7"},
        {"multiport_motors", "1"},
        {"mock_motors", "0"},
        {"restapi_port", "8080"},
        {"db_admin_pw", "test_password"}
    };
    facade->_redis.hmset("conf:env", env_settings.begin(), env_settings.end());

    // Setup test user parameters
    std::unordered_map<std::string, std::string> user_params = {
        {"motorforce_min", "10"},
        {"motorforce_max", "100"},
        {"bodyweight", "70"},
        {"upper_arm", "30"},
        {"upper_arm_cuff", "5"},
        {"forearm", "25"},
        {"forearm_cuff", "5"},
        {"cuff", "10"},
        {"motorforce", "50"},
        {"assist", "75"},
        {"selected_task", "test_task"},
        {"user_id", "1"}
    };
    facade->_redis.hmset("conf:user", user_params.begin(), user_params.end());

    // LoadEnvironmentSettings
    auto settings = facade->load_env(true);
    std::cout << "motor_e_flex: " << settings.motor_e_flex << "\n";
    std::cout << "motor_e_ext: " << settings.motor_e_ext << "\n";
    std::cout << "motor_s_flex: " << settings.motor_s_flex << "\n";
    std::cout << "motor_s_ext: " << settings.motor_s_ext << "\n";
    std::cout << "motor_s_add_pron: " << settings.motor_s_add_pron << "\n";
    std::cout << "motor_s_abd: " << settings.motor_s_abd << "\n";
    std::cout << "motor_s_add_sup: " << settings.motor_s_add_sup << "\n";
    std::cout << "multiport_motors: " << settings.multiport_motors << "\n";
    std::cout << "mock_motors: " << settings.mock_motors << "\n";
    std::cout << "restapi_port: " << settings.restapi_port << "\n";
    std::cout << "db_admin_pw: " << settings.db_admin_pw << "\n";

    // LoadUserParams
    auto params = facade->load_user_params();
    std::cout << "motorforce_min: " << params.motorforce_min << "\n";
    std::cout << "motorforce_max: " << params.motorforce_max << "\n";
    std::cout << "bodyweight: " << params.bodyweight << "\n";
    std::cout << "upper_arm: " << params.upper_arm << "\n";
    std::cout << "upper_arm_cuff: " << params.upper_arm_cuff << "\n";
    std::cout << "forearm: " << params.forearm << "\n";
    std::cout << "forearm_cuff: " << params.forearm_cuff << "\n";
    std::cout << "cuff: " << params.cuff << "\n";
    std::cout << "motorforce: " << params.motorforce << "\n";
    std::cout << "assist: " << params.assist << "\n";
    std::cout << "selected_task: " << params.selected_task << "\n";
    std::cout << "user_id: " << params.user_id << "\n";

    // UserParamsChanged
    std::cout << "user_params_changed (initial): " << facade->user_params_changed(false) << "\n";
    facade->_redis.set("conf:user:set", "1");
    std::cout << "user_params_changed (after set): " << facade->user_params_changed(false) << "\n";
    std::cout << "user_params_changed (after clear): " << facade->user_params_changed(true) << "\n";
    std::cout << "user_params_changed (final): " << facade->user_params_changed(false) << "\n";

    // DbParamsChanged
    std::cout << "db_params_changed (initial): " << facade->db_params_changed(false) << "\n";
    facade->_redis.set("dbchanged", "1");
    std::cout << "db_params_changed (after set): " << facade->db_params_changed(false) << "\n";
    std::cout << "db_params_changed (after clear): " << facade->db_params_changed(true) << "\n";
    std::cout << "db_params_changed (final): " << facade->db_params_changed(false) << "\n";

    // CurrentUserId
    auto user_id_opt = facade->current_user_id();
    if (user_id_opt.has_value())
        std::cout << "current_user_id (initial): " << user_id_opt.value() << "\n";
    else
        std::cout << "current_user_id (initial): nullopt\n";

    facade->_redis.set("currentuserid", "42");
    user_id_opt = facade->current_user_id();
    if (user_id_opt.has_value())
        std::cout << "current_user_id (after set): " << user_id_opt.value() << "\n";
    else
        std::cout << "current_user_id (after set): nullopt\n";

    facade->_redis.set("currentuserid", "invalid");
    user_id_opt = facade->current_user_id();
    if (user_id_opt.has_value())
        std::cout << "current_user_id (after invalid): " << user_id_opt.value() << "\n";
    else
        std::cout << "current_user_id (after invalid): nullopt\n";

    // MotorIds
    auto motor_ids = settings.motor_ids();
    std::cout << "motor_ids:\n";
    for (const auto& id : motor_ids)
        std::cout << "  " << id << "\n";

    // Cleanup
    facade->_redis.flushall();

    return 0;
}
