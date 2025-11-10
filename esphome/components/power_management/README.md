---
description: "Instructions for setting up Power Management component."
title: "Power Management Component"
params:
  seo:
    description: Instructions for setting up Power Management component.
---

This is a Prototype Power Management component that enables Power Management and also provided methods for acquiring and releasing Locks

[esp-idf Power Management] (https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/power_management.html)
Power management algorithm included in ESP-IDF can adjust the advanced peripheral bus (APB) frequency, CPU frequency, and put the chip into Light-sleep mode to run an application at smallest possible power consumption, given the requirements of application components.


> [!NOTE]
> This is a framework component that is currently only available for esp32-c6 and esp32-h2.  All physical testing use an esp32-h2 super mini.
> [Initial support for power management #4916](https://github.com/esphome/esphome/pull/4916) was used as a reference (@silverchris)
> Built on top of [[openthread] add poll period for mtd devices #11374](https://github.com/esphome/esphome/pull/11374)
> This branch includes modifications to API component to acquire and release locks that are tracked in the Power Management component.
> Do not use Deep Sleep component with tickless_idle configuration variable.

## Usage
Example usage including a sensor
```
esphome:
  name: test-esp32h2-2
  friendly_name: Test ESP32H2 2
  name_add_mac_suffix: false

esp32:
  board: esp32-h2-devkitm-1
  variant: ESP32H2
  flash_size: 4MB
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_OPENTHREAD_XTAL_ACCURACY: "10"
    log_level: INFO

network:
  enable_ipv6: true

# I tested using jtag which means sleep closed the serial port with error
logger:
  level: DEBUG
  hardware_uart: USB_SERIAL_JTAG
  logs:
    sensor: WARN

api:
  id: api_id
  encryption:
    key: !secret api_encryption_key

http_request:
  timeout: 180sec
  watchdog_timeout: 600sec

ota:
  - platform: http_request
    id: ota_hr_id

# this is configured as a SED with 50sec call for data packages
# do not use poll_period with a deep_sleep component
openthread:
  id: op_id
  device_type: mtd
  tlv: !secret openthread_tlv
  poll_period: 50sec

# used to determine if hardware reboots are occuring
sensor:
  - platform: uptime
    name: "Open Thread Connect"
    id: open_thread_connect_sensor_id
    type: seconds
    update_interval: 1min

# Set tickless_idle if you want auto light sleep
# Do not use tickless_idle if adding a deep_sleep component
power_management:
  id: pm_id
  initial_lock_duration: 61sec
  tickless_idle: true
  power_down_flash: true
  power_down_peripherals: true

# used to lock out Power Management
switch:
  - platform: template
    name: "Lock"
    optimistic: true
    restore_mode: RESTORE_DEFAULT_OFF
    turn_on_action:
      then:
        - logger.log: "Turn On Lock"
        - power_management.acquire_lock:
    turn_off_action:
      then:
        - logger.log: "Turn Off Lock"
        - power_management.release_lock:

button:
  - platform: template
    name: "OTA Http Request Update"
    on_press:
      then:
        - logger.log: "Turn On Lock"
        - power_management.acquire_lock:
        - logger.log: "Begin OTA Flash"
        # This is a local webserver, ota.http_request over openthread can take hours
        - ota.http_request.flash:
            md5_url: http://192.168.1.47:8000/test-esp32h2-2/.pioenvs/test-esp32h2-2/firmware.md5
            url: http://192.168.1.47:8000//test-esp32h2-2/.pioenvs/test-esp32h2-2/firmware.ota.bin
        # This only happens on a failed ota
        - logger.log: "Turn Off Lock"
        - power_management.release_lock:
```

### Configuration variables

- **initial_lock_duration** (*Optional*, [Time](#config-time)): Time that device is locked initially after boot
- **tickless_idle** (*Optional*, boolean): Stops the system's periodic tick interrupt during idle periods to reduce current consumption.
- **power_down_flash** (*Optional*, boolean): Safe power down, do not set to true if device has PSRAM.
- **power_down_peripherals** (*Optional*, boolean): For disabled peripherals, automatically save and restore peripheral states, which allows the peripherals to be powered down.
- **profiling** (*Optional*, boolean): See CONFIG_PM_PROFILING.  Only works with DEBUG logging.  Untested
- **trace** (*Optional*, boolean): See CONFIG_PM_TRACE.  Untested

## `power_management.acquire_lock` Action
This action acquires a CPU Lock

## `power_management.release_lock` Action
This action releases a CPU Lock
