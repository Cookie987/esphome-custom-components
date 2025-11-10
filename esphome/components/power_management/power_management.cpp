#include "esphome/core/log.h"
#include "power_management.h"

namespace esphome {
namespace power_management {

static const char *TAG = "power_management";

PowerManagement *global_pm = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *power_manager_user_to_string(PowerManagementLockUser user) {
  switch (user) {
    case SELF:
      return "self";
    case ACTION:
      return "action";
    case API:
      return "api";
    case OT:
      return "openthread";
    case UNKNOWN:
    default:
      return "UNKNOWN";
  }
}
void PowerManagement::setup() { this->setup_(); }

void PowerManagement::loop() { this->loop_(); }

void PowerManagement::acquire_lock(PowerManagementLockUser user, PowerManagementLockType lt) {
  this->acquire_lock_(user, lt);
}

void PowerManagement::release_lock(PowerManagementLockUser user, PowerManagementLockType lt) {
  this->release_lock_(user, lt);
}

int PowerManagement::get_total_lock_count() { return this->get_total_lock_count_(); }

void PowerManagement::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Management:");
  uint32_t duration = this->initial_lock_duration_ / 1000;
  ESP_LOGCONFIG(TAG, "  Initial Lock Duration: %" PRIu32 "s", duration);
  this->dump_config_();
}

}  // namespace power_management
}  // namespace esphome
