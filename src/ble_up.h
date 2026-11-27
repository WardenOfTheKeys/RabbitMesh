// RabbitMesh: the Bluetooth stack up for a scan. MeshCore's phone link sets it up at boot
// (node.cpp), but with Bluetooth off in settings the firmware parks the controller and host
// for light sleep (blePrepareLightSleep) and only brings them back when Bluetooth is turned on
// again; a scan on a parked stack does nothing. This resumes them without turning the phone
// link's advertising on. The next nap parks them again if Bluetooth is still off, and naps
// don't happen while the wardrive or the flock hunt runs (they keep the GPS on).
#pragma once
#include <esp_bt.h>
#include <esp_bt_main.h>

namespace rabbit_ble {
inline bool up() {
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED &&
      esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) return false;
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED &&
      esp_bluedroid_enable() != ESP_OK) return false;
  return esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED;
}
}  // namespace rabbit_ble
