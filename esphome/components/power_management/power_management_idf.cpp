#include "power_management.h"
#ifdef USE_ESP_IDF
#include "esphome/core/log.h"

namespace esphome {
namespace power_management {

static const char *TAG = "power_management";

// static only use is in setup
void PowerManagement::initial_timer_callback(TimerHandle_t xTimer) {
  void *context = pvTimerGetTimerID(xTimer);
  PowerManagement *obj = (PowerManagement *) context;
  obj->initial_lock_ = false;
  obj->release_lock(PowerManagementLockUser::SELF, PowerManagementLockType::CPU);
}

void PowerManagement::setup_() {
  power_management::global_pm = this;
  esp_err_t rc = ESP_OK;
  // Configure PM
  int max_freq_mhz = this->max_freq_mhz_ > 0 ? this->max_freq_mhz_ : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
  int min_freq_mhz = this->min_freq_mhz_ > 0 ? this->min_freq_mhz_ : esp_clk_xtal_freq() / MHZ;

  esp_pm_config_t pm_config = {
    .max_freq_mhz = max_freq_mhz,
    .min_freq_mhz = min_freq_mhz,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    .light_sleep_enable = true
#endif
  };
  ESP_LOGI(TAG, "PM Max Freq: %dMHZ", pm_config.max_freq_mhz);
  ESP_LOGI(TAG, "PM Min Freq: %dMHZ", pm_config.min_freq_mhz);
  ESP_LOGI(TAG, "PM Light Sleep Enable: %s", pm_config.light_sleep_enable ? "true" : "false");

  rc = esp_pm_configure(&pm_config);
  if (rc != 0) {
    this->mark_failed();
    ESP_LOGE(TAG, "Failed esp_pm_configure %d", rc);
    return;
  }

  // Create Locks
  rc = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "esphome_cpu", &this->pm_pm_lock_cpu_);
  if (rc != ESP_OK) {
    esp_pm_lock_delete(this->pm_pm_lock_cpu_);
    this->pm_pm_lock_cpu_ = NULL;
    ESP_LOGE(TAG, "Failed esp_pm_lock_create %d", rc);
    return;
  }
  rc = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "esphome_apb", &this->pm_pm_lock_apb_);
  if (rc != ESP_OK) {
    esp_pm_lock_delete(this->pm_pm_lock_apb_);
    this->pm_pm_lock_apb_ = NULL;
    ESP_LOGE(TAG, "Failed esp_pm_lock_create %d", rc);
    return;
  }
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  rc = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "esphome_slp", &this->pm_pm_lock_slp_);
  if (rc != ESP_OK) {
    esp_pm_lock_delete(this->pm_pm_lock_slp_);
    this->pm_pm_lock_slp_ = NULL;
    ESP_LOGE(TAG, "Failed esp_pm_lock_create %d", rc);
    return;
  }
#endif

  if (this->initial_lock_duration_ > 0) {
    this->initial_lock_ = true;
    // Acquire Initial Lock
    this->acquire_lock(PowerManagementLockUser::SELF, PowerManagementLockType::CPU);
    TimerHandle_t xTimer = xTimerCreate("PM Sleep Timer", pdMS_TO_TICKS(this->initial_lock_duration_), pdFALSE, this,
                                        PowerManagement::initial_timer_callback);
    xTimerStart(xTimer, 0);
  }
}

void PowerManagement::loop_() {
  if (this->is_dump_locks_ && this->get_total_lock_count_() == 0) {
    ESP_LOGD(TAG, "PM Locks Dumped", esp_pm_dump_locks(stdout));
    this->is_dump_locks_ = false;
  } else if (!this->is_dump_locks_ && this->get_total_lock_count_() > 0) {
    this->is_dump_locks_ = true;
  }
}

// Not Thread Safe, but read only
int PowerManagement::get_total_lock_count_() {
  if (this->is_ready()) {
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    return this->pm_lock_counter_cpu_ + this->pm_lock_counter_apb_ + this->pm_lock_counter_slp_;
#else
    return this->pm_lock_counter_cpu_ + this->pm_lock_counter_apb_;
#endif
  }
  return 0;
}

// Thread Safe
void PowerManagement::acquire_lock_(PowerManagementLockUser user, PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    switch (lt) {
      case PowerManagementLockType::CPU:
        if (this->pm_lock_counter_cpu_ == 0) {
          esp_err_t rc = esp_pm_lock_acquire(this->pm_pm_lock_cpu_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_acquire %d", rc);
          }
        }
        pm_lock_counter_cpu_++;
        ESP_LOGD(TAG, "Acquired Power Management CPU Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_cpu_, this->get_total_lock_count_());
        break;
      case PowerManagementLockType::APB:
        if (this->pm_lock_counter_apb_ == 0) {
          esp_err_t rc = esp_pm_lock_acquire(this->pm_pm_lock_apb_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_acquire %d", rc);
          }
        }
        this->pm_lock_counter_apb_++;
        ESP_LOGD(TAG, "Acquired Power Management APB Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_apb_, this->get_total_lock_count_());
        break;
      case PowerManagementLockType::SLP:
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        if (this->pm_lock_counter_slp_ == 0) {
          esp_err_t rc = esp_pm_lock_acquire(this->pm_pm_lock_slp_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_acquire %d", rc);
          }
        }
        this->pm_lock_counter_slp_++;
        ESP_LOGD(TAG, "Acquired Power Management SLP Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_slp_, this->get_total_lock_count_());
#endif
        break;
      default:
        ESP_LOGW(TAG, "Lock Type undefined");
        break;
    }
  }
}

// Thread Safe
void PowerManagement::release_lock_(PowerManagementLockUser user, PowerManagementLockType lt) {
  if (this->is_ready()) {
    std::lock_guard<std::mutex> lock(this->pm_lock_mutex_);
    switch (lt) {
      case PowerManagementLockType::CPU:
        // this assumes that initial lock is cpu, prevents a button from accidentally removing lock
        if (this->initial_lock_ && this->pm_lock_counter_cpu_ == 1) {
          break;
        }
        if (this->pm_lock_counter_cpu_ == 1) {
          esp_err_t rc = esp_pm_lock_release(this->pm_pm_lock_cpu_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_release %d", rc);
          }
        }
        if (this->pm_lock_counter_cpu_ > 0) {
          this->pm_lock_counter_cpu_--;
        }
        ESP_LOGD(TAG, "Released Power Management CPU Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_cpu_, this->get_total_lock_count_());
        break;
      case PowerManagementLockType::APB:
        if (this->pm_lock_counter_apb_ == 1) {
          esp_err_t rc = esp_pm_lock_release(this->pm_pm_lock_apb_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_release %d", rc);
          }
        }
        if (this->pm_lock_counter_apb_ > 0) {
          this->pm_lock_counter_apb_--;
        }
        ESP_LOGD(TAG, "Released Power Management APB Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_apb_, this->get_total_lock_count_());
        break;
      case PowerManagementLockType::SLP:
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        if (this->pm_lock_counter_slp_ == 1) {
          esp_err_t rc = esp_pm_lock_release(this->pm_pm_lock_slp_);
          if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed esp_pm_lock_release %d", rc);
          }
        }
        if (this->pm_lock_counter_slp_ > 0) {
          this->pm_lock_counter_slp_--;
        }
        ESP_LOGD(TAG, "Released Power Management SLP Lock, user: %s, cnt: %d, total_cnt: %d",
                 power_manager_user_to_string(user), this->pm_lock_counter_slp_, this->get_total_lock_count_());
#endif
        break;
      default:
        ESP_LOGW(TAG, "Lock Type undefined");
    }
  }
}

void PowerManagement::dump_config_() {
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  ESP_LOGCONFIG(TAG, "  Light Sleep Enabled");
#if CONFIG_ESP_SLEEP_POWER_DOWN_FLASH
  ESP_LOGCONFIG(TAG, "  PM Flash Power Down in Light Sleep Enabled");
#endif
#if CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP
  ESP_LOGCONFIG(TAG, "  PM Peripheral Power Down in Light Sleep Enabled");
#endif
#if CONFIG_IEEE802154_SLEEP_ENABLE
  ESP_LOGCONFIG(TAG, "  ieee802154 Sleep Enabled");
#endif
#if CONF_PM_PROFILING
  ESP_LOGCONFIG(TAG, "  PM Profiling Enabled");
#endif
#if CONF_PM_TRACE
  ESP_LOGCONFIG(TAG, "  PM Trace Enabled");
#endif
#endif
}

}  // namespace power_management
}  // namespace esphome
#endif
