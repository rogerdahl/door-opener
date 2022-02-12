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
#include "nvs_flash.h"
#include <assert.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "door_state.h"
#include "door_task.h"
#include "int_t.h"
#include "pins.h"

#include "now.h"

static void      init_wifi();
static esp_err_t init_espnow(espnow_send_param_t* send_param);
// static void      espnow_deinit(espnow_send_param_t* send_param);
// static void      espnow_task(void* pvParameter);

static void send_status_cb(const uint8_t* mac_addr, esp_now_send_status_t status);
static void recv_cb(const uint8_t* mac_addr, const uint8_t* data, int len);

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// static uint16_t s_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };

static const char* TAG = "now";

typedef char         NowMsg[32];
static QueueHandle_t sendQueue;
static QueueHandle_t recvQueue;
// static QueueHandle_t statusQueue;

static void send_task(void* pvParameter);
// static void recv_task(void* pvParameter);

void init_now(QueueHandle_t* sendQueue_, QueueHandle_t* recvQueue_) {
  init_wifi();

  espnow_send_param_t* send_param = (espnow_send_param_t*)malloc(sizeof(espnow_send_param_t));
  init_espnow(send_param);

  *sendQueue_ = xQueueCreate(32, sizeof(NowMsg));
  *recvQueue_ = xQueueCreate(32, sizeof(NowMsg));

  sendQueue = *sendQueue_;
  recvQueue = *recvQueue_;

  xTaskCreate(send_task, "send_task", 8192, (void*)send_param, tskIDLE_PRIORITY + 1, nullptr);
  // xTaskCreate(recv_task, "recv_task", 8192, (void*)send_param, tskIDLE_PRIORITY + 1, nullptr);
}

// Monitor the send queue and send messages out.
static void send_task(void* pvParameter) {
  espnow_send_param_t* send_param = (espnow_send_param_t*)pvParameter;
  NowMsg               nowMsg;
  while (true) {
    xQueueReceive(sendQueue, &nowMsg, portMAX_DELAY);
    ESP_LOGD(TAG, "Sending: %s", nowMsg);
    if (esp_now_send(send_param->dest_mac, (u8*)nowMsg, sizeof(NowMsg)) != ESP_OK) {
      ESP_LOGE(TAG, "Send error");
    }
  }
}

// Wi-Fi should start before using ESPNOW.
static void init_wifi() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
  ESP_ERROR_CHECK(esp_wifi_start());

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
  ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF,
                                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}

static esp_err_t init_espnow(espnow_send_param_t* send_param) {
  ESP_ERROR_CHECK(esp_now_init());

  // The send callback is called after the send operation and allows checking the status of the operation. Registering
  // this callback is optional.
  ESP_ERROR_CHECK(esp_now_register_send_cb(send_status_cb));
  ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));

  // Set primary master key
  ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t*)CONFIG_ESPNOW_PMK));

  // Add broadcast peer information to peer list.
  esp_now_peer_info_t* peer = (esp_now_peer_info_t*)malloc(sizeof(esp_now_peer_info_t));
  if (peer == nullptr) {
    ESP_LOGE(TAG, "Malloc peer information failed");
    esp_now_deinit();
    return ESP_FAIL;
  }
  memset(peer, 0, sizeof(esp_now_peer_info_t));
  peer->channel = CONFIG_ESPNOW_CHANNEL;
  peer->ifidx   = (wifi_interface_t)ESPNOW_WIFI_IF;
  peer->encrypt = false;
  memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK(esp_now_add_peer(peer));
  free(peer);

  // Initialize sending parameters.
  memset(send_param, 0, sizeof(espnow_send_param_t));
  if (send_param == nullptr) {
    ESP_LOGE(TAG, "Malloc send parameter failed");
    esp_now_deinit();
    return ESP_FAIL;
  }
  send_param->unicast   = false;
  send_param->broadcast = true;
  send_param->state     = 0;
  send_param->magic     = esp_random();
  send_param->count     = CONFIG_ESPNOW_SEND_COUNT;
  send_param->delay     = CONFIG_ESPNOW_SEND_DELAY;
  send_param->len       = CONFIG_ESPNOW_SEND_LEN;
  send_param->buffer    = (uint8_t*)malloc(CONFIG_ESPNOW_SEND_LEN);
  if (send_param->buffer == nullptr) {
    ESP_LOGE(TAG, "Malloc send buffer failed");
    free(send_param);
    esp_now_deinit();
    return ESP_FAIL;
  }
  memcpy(send_param->dest_mac, s_broadcast_mac, ESP_NOW_ETH_ALEN);

  // xTaskCreate(espnow_task, "espnow_task", 2048, send_param, 4, nullptr);
  // xTaskCreate(test_task, "test_task", 8192, send_param, 4 + 1, nullptr);

  return ESP_OK;
}

// static void espnow_deinit(espnow_send_param_t* send_param) {
//  free(send_param->buffer);
//  free(send_param);
//  esp_now_deinit();
//}

// ESPNOW send and receive callbacks are called from a high priority Wi-Fi task. Lengthy operations should not be done
// from these callbacks. Instead, post any necessary data to a queue and handle it from a lower priority task.

// The send callback is called after the send operation and allows checking the status of the operation. Registering
// this callback is optional.
static void send_status_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {
  espnow_event_t          evt;
  espnow_event_send_cb_t* send_cb = &evt.info.send_cb;

  if (mac_addr == nullptr) {
    ESP_LOGE(TAG, "Send cb arg error");
    return;
  }

  evt.id = EXAMPLE_ESPNOW_SEND_CB;
  memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
  send_cb->status = status;

  // Safe?
  ESP_LOGD(TAG, "Send status: %d", status);

  // This would be the right place to post a message to a queue for sending an ack back to the sender.

  // xQueueSend(sendStatusQueueHandle, &sendStatus, portMAX_DELAY);
  //
  // SendStatusMsg sendStatus;
  // sprintf(sendStatus, "Status %d", status);
  // xQueueSend(sendStatusQueueHandle, &sendStatus, portMAX_DELAY);
}

// buf->payload = 1;
// espnow_data_t *buf = (espnow_data_t *)send_param->buffer;
// assert(send_param->len >= sizeof(espnow_data_t));

// Receive messages and add them to the receive queue.
static void recv_cb(const uint8_t* mac_addr, const uint8_t* data, int len) {
  if (mac_addr == NULL || data == NULL || len <= 0) {
    ESP_LOGE(TAG, "Receive cb arg error");
    return;
  }

  xQueueSend(recvQueue, (void*)data, portMAX_DELAY);
}

// static void recv_task(void* pvParameter) {
//   espnow_send_param_t* send_param = (espnow_send_param_t*)pvParameter;
//   NowMsg nowMsg;
//   while (true) {
//     xQueueReceive(recvQueue, &nowMsg, portMAX_DELAY);
//     ESP_LOGD(TAG, "Sending: %s", nowMsg);
//     if (esp_now_send(send_param->dest_mac, (u8*)sendStatus, sizeof(SendStatusMsg)) != ESP_OK) {
//       ESP_LOGE(TAG, "Send error");
//     }
//   }
// }
