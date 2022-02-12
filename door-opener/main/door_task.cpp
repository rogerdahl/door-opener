#include "esp_log.h"

#include "door_state.h"
#include "door_task.h"

QueueHandle_t queueHandle;
TimerHandle_t timerHandle = xTimerCreate("Timer", 1, false, NULL, timer_callback);

struct CmdAndArg {
  Cmd cmd;
} cmdAndArg;

static const char* TAG = "task";

Door door(queueHandle);

void init_state_machine() {
  queueHandle = xQueueCreate(32, sizeof(cmdAndArg));
  xTaskCreate(door_state_task, "door_state_task", 8192, NULL, tskIDLE_PRIORITY + 1, NULL);
}

void trigger_open() {
  enqueue({CMD_OPEN});
}

void schedule_next(u32 delay_ms) {
  ESP_LOGI(TAG, "Scheduling call to Next() after: %dms", delay_ms);
  xTimerChangePeriod(timerHandle, pdMS_TO_TICKS(delay_ms), 0);
  xTimerStart(timerHandle, 0);
}

void cancel_next() {
  ESP_LOGI(TAG, "Cancelling any scheduled call to Next()");
  xTimerStop(timerHandle, 0);
}

void timer_callback(TimerHandle_t xTimerHandle) {
  enqueue({CMD_NEXT});
}

void enqueue(const CmdAndArg& ev) {
  xQueueSend(queueHandle, &ev, 0);
}

void door_state_task(__attribute__((unused)) void* params) {
  CmdAndArg ev = {CMD_NONE};

  while (true) {
    ESP_LOGD(TAG, "Starting door_state task...");
    while (true) {
      // Block until something arrives in the queue.
      xQueueReceive(queueHandle, &ev, portMAX_DELAY);
      switch (ev.cmd) {
      case Cmd::CMD_NONE:
        ESP_LOGD(TAG, "CMD_NONE");
        break;
      case Cmd::CMD_OPEN:
        ESP_LOGD(TAG, "CMD_OPEN");
        door.Open();
        break;
      case Cmd::CMD_NEXT:
        ESP_LOGD(TAG, "CMD_NEXT");
        door.Next();
        break;
      default:
        ESP_LOGE(TAG, "Unhandled case");
        break;
      }
    }
  }
}
