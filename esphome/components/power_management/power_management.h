#pragma once

#include "esphome/core/component.h"

#ifdef USE_ESP32
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "soc/rtc.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <mutex>
#endif

namespace esphome {
namespace power_management {

enum PowerManagementLockType {
  CPU = 0,
  APB = 1,
  SLP = 2,
};

enum PowerManagementLockUser : uint8_t {
  UNKNOWN = 0,
  SELF = 1,
  ACTION = 2,
  API = 3,
  OT = 4,
};

const char *power_manager_user_to_string(PowerManagementLockUser user);

class PowerManagement : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_initial_lock_duration(uint32_t initial_lock_duration) {
    this->initial_lock_duration_ = initial_lock_duration;
  }
  void set_max_freq_mhz(uint32_t max_freq_mhz) { this->max_freq_mhz_ = max_freq_mhz; }
  void set_min_freq_mhz(uint32_t min_freq_mhz) { this->min_freq_mhz_ = min_freq_mhz; }
  void acquire_lock(PowerManagementLockUser user, PowerManagementLockType lt);
  void release_lock(PowerManagementLockUser user, PowerManagementLockType lt);
  int get_total_lock_count();
#ifdef USE_ESP32
  static void initial_timer_callback(TimerHandle_t xTimer);
#endif
 protected:
  void setup_();
  void loop_();
  void dump_config_();
  void acquire_lock_(PowerManagementLockUser user, PowerManagementLockType lt);
  void release_lock_(PowerManagementLockUser user, PowerManagementLockType lt);
  int get_total_lock_count_();
#ifdef USE_ESP32
  bool is_dump_locks_{false};
  int pm_lock_counter_cpu_;
  esp_pm_lock_handle_t pm_pm_lock_cpu_{NULL};
  int pm_lock_counter_apb_;
  esp_pm_lock_handle_t pm_pm_lock_apb_{NULL};
// Lock is redundant if not tickless
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  int pm_lock_counter_slp_;
  esp_pm_lock_handle_t pm_pm_lock_slp_{NULL};
#endif
  mutable std::mutex pm_lock_mutex_;
#endif
  uint32_t initial_lock_duration_{0};
  bool initial_lock_{false};
  uint32_t max_freq_mhz_{0};
  uint32_t min_freq_mhz_{0};
};

extern PowerManagement *global_pm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace power_management
}  // namespace esphome
