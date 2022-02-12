#include "driver/gpio.h"
#include "esp_crc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "now.h"
#include "nvs_flash.h"
#include <assert.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "counter.h"
#include "door_state.h"
#include "door_task.h"
#include "int_t.h"
#include "pins.h"

#define ESPNOW_MAXDELAY       512
#define ESP_INTR_FLAG_DEFAULT 0

static volatile bool pir_inside  = false;
static volatile bool pir_outside = false;
static volatile bool remote_open = false;

static const char* TAG = "main";

static void main_task(void* pvParameter);
static void remote_task(void* pvParameter);

void        init_pir();

void init_relays();

// void set_relays(int r1, int r2);
// void start_open();
// void start_close();
// void stop();
void init_beep();

void beep(u32 duration_ms);

void gpio_isr_pir_handler(void* pir_num_in);

void print_chip_info();

void init_nvs();

extern "C" {
void app_main();
}

// struct det {
//   int test = {0};
// };

// u8 det = 0;

// StaticSemaphore_t ignoreFalseTrigger;
SemaphoreHandle_t ignoreFalseTrigger;

typedef char Msg[32];
// QueueHandle_t msgQueueHandle = xQueueCreate(32, sizeof(Msg));

typedef char SendStatusMsg[32];
// char[32] sendStatus;
QueueHandle_t sendStatusQueueHandle = xQueueCreate(32, sizeof(SendStatusMsg));

static QueueHandle_t nowSendQueue;
static QueueHandle_t nowRecvQueue;

void app_main() {
  print_chip_info();
  // Initialize shared hardware resources.
  init_nvs();
  init_beep();
  init_relays();
  init_pir();
  init_state_machine();
  // trigger_open();

  ignoreFalseTrigger = xSemaphoreCreateBinary();
  xSemaphoreGive(ignoreFalseTrigger);

  init_now(&nowSendQueue, &nowRecvQueue);
  xTaskCreate(main_task, "main_task", 8192, nullptr, tskIDLE_PRIORITY, nullptr);
  xTaskCreate(remote_task, "remote_task", 8192, nullptr, tskIDLE_PRIORITY, nullptr);

  //  pir_inside=true;
}

static void main_task(void* pvParameter) {
  // espnow_send_param_t* send_param = (espnow_send_param_t*)pvParameter;
  Msg msg;

  //  vTaskDelay(pdMS_TO_TICKS(5000));

  while (true) {
    snprintf(msg, sizeof(Msg), "<no msg>");

    if (pir_inside || pir_outside || remote_open) {
      if (pir_inside) {
        snprintf(msg, sizeof(Msg), "PIR inside");
        pir_inside  = false;
      }
      if (pir_outside) {
        snprintf(msg, sizeof(Msg), "PIR outside");
        pir_outside = false;
      }
      if (remote_open) {
        snprintf(msg, sizeof(Msg), "Remote open");
        remote_open = false;
      }

      ESP_LOGI(TAG,"%s", msg);

      if (!xSemaphoreTake(ignoreFalseTrigger, 0)) {
        ESP_LOGI(TAG, "PIR ignored: Probably triggered by relays");
        continue;
      }

      xSemaphoreGive(ignoreFalseTrigger);

      // beep(30);
      trigger_open();

      ESP_LOGI(TAG, "Door has been opened %d times", get_door_opened_counter());

      if (xQueueSend(nowSendQueue, msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "nowSendQueue failed");
      }

      // ESP_LOGI(TAG, "Sending: %d", det);
      // if (esp_now_send(send_param->dest_mac, (u8*)msg, sizeof(Msg)) != ESP_OK) {
      //   ESP_LOGE(TAG, "Send error");
      //   espnow_deinit(send_param);
      //   vTaskDelete(nullptr);
      // }
    }

    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

static void remote_task(__attribute__((unused)) void* params) {
  Msg msg;
  while (true) {
   xQueueReceive(nowRecvQueue, &msg, portMAX_DELAY);
   ESP_LOGI(TAG, "Received from remote: %s", msg);
   trigger_open();
  }
}

// static void test_task(void* pvParameter) {
//   espnow_send_param_t* send_param = (espnow_send_param_t*)pvParameter;
//   SendStatusMsg        sendStatus;
//   u32                  c = 0;
//   while (true) {
//     snprintf(sendStatus, sizeof(SendStatusMsg), "test %d", c++);
//     if (esp_now_send(send_param->dest_mac, (u8*)sendStatus, sizeof(SendStatusMsg)) != ESP_OK) {
//       ESP_LOGE(TAG, "ESPNOW test error");
//     } else {
//       ESP_LOGE(TAG, "ESPNOW sent test msg: %s", sendStatus);
//     }
//     vTaskDelay(pdMS_TO_TICKS(5000));
//   }

// Non-volatile (key-value storage)
//
// From esp32.com: NVS keeps key-value pairs in a log-based structure, which works well
// for minimizing flash wear. It works best when you use small values, i.e. 8-64 bit
// integers. For blobs and strings, especially large ones (1k and above), performance is
// worse. With integers, you can expect one flash sector to be erased once per every 125
// updates of a value in key-value pair.
void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void print_chip_info() {
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("ESP32 with %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
  printf("Silicon revision %d, ", chip_info.revision);
  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

// PIR

void init_pir() {
  gpio_config_t io_conf = {};
  io_conf.intr_type     = GPIO_INTR_POSEDGE;
  io_conf.pin_bit_mask  = GPIO_SEL_PIR_INSIDE | GPIO_SEL_PIR_OUTSIDE;
  io_conf.mode          = GPIO_MODE_INPUT;
  io_conf.pull_up_en    = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
  // install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  // hook isr handler for specific gpio pin
  gpio_isr_handler_add(GPIO_NUM_PIR_INSIDE, gpio_isr_pir_handler, (void*)1);
  gpio_isr_handler_add(GPIO_NUM_PIR_OUTSIDE, gpio_isr_pir_handler, (void*)2);
}

void gpio_isr_pir_handler(void* pir_num_in) {
  int pir = (int)pir_num_in;
  if (pir == 1) {
    pir_inside = true;
  }
  if (pir == 2) {
    pir_outside = true;
  }
}

// relays

void init_relays() {
  gpio_config_t io_conf = {};
  memset(&io_conf, 0, sizeof(io_conf));
  io_conf.intr_type    = GPIO_INTR_DISABLE;
  io_conf.pin_bit_mask = GPIO_SEL_RELAY1 | GPIO_SEL_RELAY2;
  io_conf.mode         = GPIO_MODE_OUTPUT;
  gpio_config(&io_conf);
}

// beep

void init_beep() {
  gpio_config_t io_conf = {};
  memset(&io_conf, 0, sizeof(io_conf));
  io_conf.intr_type    = GPIO_INTR_DISABLE;
  io_conf.pin_bit_mask = GPIO_SEL_BEEP;
  io_conf.mode         = GPIO_MODE_OUTPUT;
  gpio_config(&io_conf);
}

void beep(u32 duration_ms) {
  gpio_set_level(GPIO_NUM_BEEP, 1);
  vTaskDelay(pdMS_TO_TICKS(duration_ms));
  gpio_set_level(GPIO_NUM_BEEP, 0);
}
