// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wdeprecated-declarations"
// #pragma ide diagnostic ignored "EmptyDeclOrStmt"
// #pragma ide diagnostic ignored "EndlessLoop"
// #pragma ide diagnostic ignored "UnusedParameter"

extern "C" {
void app_main(void);
}

#include <cstdio>
#include <cstring>

#include "driver/uart.h"
#include "esp_console.h"
// #include "esp_hid_host_main.h"
#include "esp_vfs_dev.h"
#include "m5display.h"
#include "m5stickc.h"
#include "util/tft.h"
#include <esp_log.h>

#include "freertos/queue.h"
#include "now.h"

// #include "driver/gpio.h"
// #include "esp_crc.h"
#include "esp_event.h"
// #include "esp_log.h"
// #include "esp_netif.h"
#include "esp_now.h"
// #include "esp_system.h"
// #include "esp_wifi.h"
// #include "espnow.h"
#include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include <assert.h>
// #include <freertos/queue.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include "tft.h"
#include <time.h>

void initialize_console();
void button_a_click_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
void button_b_click_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);

// LED
void         startLedFlash();
void         stopLedFlash();
void         ledToggleTask(void* params_in);
TaskHandle_t led_toggle_task_handle = nullptr;

void init_hid_host();

static void remote_task(__attribute__((unused)) void* params);

void display_msg(char*);

void init_nvs();
static const char* TAG = "main";
typedef char       Msg[32];

QueueHandle_t        espNowQueueHandle;
static QueueHandle_t nowSendQueue;
static QueueHandle_t nowRecvQueue;

void app_main(void) {
  // Initialize M5StickC
  // This initializes the event loop, power, button and display
  m5stickc_config_t m5config = M5STICKC_CONFIG_DEFAULT();
  // Set starting backlight level
  m5config.power.lcd_backlight_level = 3;
  m5_init(&m5config);
  initialize_console();
  //  startLedFlash();
  m5display_timeout(5);
  esp_event_handler_register_with(m5_event_loop, m5button_a.esp_event_base, M5BUTTON_BUTTON_CLICK_EVENT,
                                  button_a_click_handler, nullptr);

  esp_event_handler_register_with(m5_event_loop, m5button_b.esp_event_base, M5BUTTON_BUTTON_CLICK_EVENT,
                                  button_b_click_handler, nullptr);
  // m5button_enable_interrupt(&m5button_a);
  // my_connect();
  // audio_init();
  init_nvs();
  init_now(&nowSendQueue, &nowRecvQueue);

  //  xTaskCreate(main_task, "main_task", 8192, nullptr, tskIDLE_PRIORITY, nullptr);
  xTaskCreate(remote_task, "remote_task", 8192, nullptr, tskIDLE_PRIORITY, nullptr);
}

static void remote_task(__attribute__((unused)) void* params) {
  Msg msg;
  while (true) {
    xQueueReceive(nowRecvQueue, &msg, portMAX_DELAY);
    ESP_LOGI(TAG, "Received from remote: %s", msg);
    display_msg(msg);
  }
}

// Buttons

void button_a_click_handler(void* _handler_arg, esp_event_base_t _base, int32_t _id, void* _event_data) {
//  stopLedFlash();
  display_msg("OPEN");
  if (xQueueSend(nowSendQueue, "OPEN", portMAX_DELAY) != pdTRUE) {
    ESP_LOGW(TAG, "nowSendQueue failed");
  }
}

void button_b_click_handler(void* _handler_arg, esp_event_base_t _base, int32_t _id, void* _event_data) {
//  display_msg("B");
}

void display_msg(char* msg) {
  TFT_fillScreen(TFT_BLACK);
  TFT_setFont(DEJAVU24_FONT, nullptr);
  TFT_print(msg, CENTER, CENTER);
  // Display is turned off again after the time set with m5display_timeout().
  m5display_wakeup();
}

// Flash LED

void startLedFlash() {
  ESP_LOGD(TAG, "Starting LED flash");
  xTaskCreate(ledToggleTask, "ledToggleTask", 1024, nullptr, tskIDLE_PRIORITY, &led_toggle_task_handle);
  configASSERT(led_toggle_task_handle);
}

void stopLedFlash() {
  ESP_LOGD(TAG, "Stopping LED flash");
  if (led_toggle_task_handle != nullptr) {
    m5led_set(true);
    vTaskDelete(led_toggle_task_handle);
  }
  led_toggle_task_handle = nullptr;
}

void ledToggleTask(void* params_in) {
  TickType_t last_toggle_time = xTaskGetTickCount();
  for (;;) {
    m5led_toggle();
    // vTaskDelayUntil is made for use in a loop and keeps exact time by taking including the time taken to run the
    // code in the delay.
    vTaskDelayUntil(&last_toggle_time, pdMS_TO_TICKS(500));
  }
}

void init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void initialize_console() {
  // Drain stdout before reconfiguring it
  fflush(stdout);
  fsync(fileno(stdout));
  // Disable buffering on stdin
  setvbuf(stdin, nullptr, _IONBF, 0);
  // Minicom, screen, idf_monitor send CR when ENTER key is pressed
  esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  // Move the caret to the beginning of the next line on '\n'
  esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
  // Configure UART. Note that REF_TICK is used so that the baud rate remains
  // correct while APB frequency is changing in light sleep mode.
  const uart_config_t uart_config = {
      .baud_rate           = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
      .data_bits           = UART_DATA_8_BITS,
      .parity              = UART_PARITY_DISABLE,
      .stop_bits           = UART_STOP_BITS_1,
      .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 2,
      .source_clk          = UART_SCLK_REF_TICK,
  };
  // Install UART driver for interrupt-driven reads and writes
  ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, nullptr, 0));
  ESP_ERROR_CHECK(uart_param_config((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
  // Tell VFS to use UART driver
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
  // Initialize the console
  esp_console_config_t console_config = {
      .max_cmdline_length = 256, .max_cmdline_args = 8, .hint_color = atoi(LOG_COLOR_CYAN), .hint_bold = 1};
  ESP_ERROR_CHECK(esp_console_init(&console_config));
}
