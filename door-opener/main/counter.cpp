#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <nvs_handle.hpp>
#include <stdio.h>

#include "counter.h"

static const char* TAG = "counter";

// std::shared_ptr<NVSHandle> open_nvs();

void inc_door_opened_counter() {
  auto cnt = get_door_opened_counter();

  esp_err_t err;
  // Handle will automatically close when going out of scope or when it's reset.
  auto handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &err);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
  }

  err = handle->set_item("cnt", ++cnt);
  switch (err) {
  case ESP_OK:
    ESP_LOGD(TAG, "Wrote door opened counter: %d", cnt);
    break;
  case ESP_ERR_NVS_NOT_FOUND:
    ESP_LOGD(TAG, "The value is not initialized yet!");
    break;
  default:
    ESP_LOGD(TAG, "Error writing door opened counter: %s", esp_err_to_name(err));
  }
  // Commit written value. After setting any values, nvs_commit() must be called to
  // ensure changes are written to flash storage. Implementations may write to storage
  // at other times, but this is not guaranteed.
  err = handle->commit();
  switch (err) {
  case ESP_OK:
    ESP_LOGD(TAG, "Committed door opened counter: %d", cnt);
    break;
  default:
    ESP_LOGD(TAG, "Error writing door opened counter: %s", esp_err_to_name(err));
  }
}

u32 get_door_opened_counter() {
  esp_err_t Err;
  // Handle will automatically close when going out of scope or when it's reset.
  auto handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &Err);
  if (Err != ESP_OK) {
    ESP_LOGD(TAG, "Error opening NVS handle: %s", esp_err_to_name(Err));
  }

  // value will default to 0, if not set yet in NVS
  u32 cnt = 0;
  switch (handle->get_item("cnt", cnt)) {
  case ESP_OK:
    ESP_LOGD(TAG, "Read door opened counter: %d", cnt);
    break;
  case ESP_ERR_NVS_NOT_FOUND:
    ESP_LOGD(TAG, "The value is not initialized yet!");
    break;
  default:
    ESP_LOGD(TAG, "Error reading door opened counter: %s", esp_err_to_name(handle->get_item("cnt", cnt)));
  }

  return cnt;
}
