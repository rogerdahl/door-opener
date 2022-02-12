#include "driver/gpio.h"
#include "esp_log.h"
#include <assert.h>

#include "counter.h"
#include "door_state.h"
#include "door_task.h"
#include "pins.h"

using namespace std;

static const char* TAG = "door";

extern SemaphoreHandle_t ignoreFalseTrigger;
extern TimerHandle_t     timerHandle;

Door::Door(QueueHandle_t queueHandle) : StateMachine(ST_MAX_STATES), queueHandle(queueHandle) {
}

// Pattern for event that takes a value:
// void Door::Open(DoorData *pData) {
// ...
// END_TRANSITION_MAP(pData)

void Door::Open() {
  BEGIN_TRANSITION_MAP                    // - Current State -
      TRANSITION_MAP_ENTRY(ST_OPENING)    // ST_Idle
      TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_Opening
      TRANSITION_MAP_ENTRY(ST_OPEN)       // ST_Open - Timer restart
      TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_Closing - Ignored because motion may be triggered by door moving
      TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_Close - May need to add "settle" here, to catch motion triggered by the
                                          // door but which arrive just after the door stopped moving.
      // TRANSITION_MAP_ENTRY(EVENT_IGNORED)    // ST_Settle
      END_TRANSITION_MAP(NULL)
}

void Door::Next() {
  BEGIN_TRANSITION_MAP                    // - Current State -
      TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_Idle
      TRANSITION_MAP_ENTRY(ST_OPEN)       // ST_Opening
      TRANSITION_MAP_ENTRY(ST_CLOSING)    // ST_Open
      TRANSITION_MAP_ENTRY(ST_CLOSED)     // ST_Closing
      TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_Close
      // TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_Settle
      END_TRANSITION_MAP(NULL)
}

// void Door::ST_Idle(EventData *pData) {
void Door::ST_Idle(DoorData*) {
  ESP_LOGD(TAG, "ST_Idle");
  cancel_next();
}

void Door::ST_Opening(DoorData*) {
  ESP_LOGD(TAG, "ST_Opening");
  start_open();
  inc_door_opened_counter();
  schedule_next(OPENING_DURATION_MS);
}

void Door::ST_Open(DoorData*) {
  ESP_LOGD(TAG, "ST_Open");
  stop();
  schedule_next(OPEN_DURATION_MS);
}

void Door::ST_Closing(DoorData*) {
  ESP_LOGD(TAG, "ST_Closing");
  start_close();
  schedule_next(CLOSING_DURATION_MS);
}

void Door::ST_Closed(DoorData*) {
  ESP_LOGD(TAG, "ST_Closed");
  stop();
  cancel_next();
  InternalEvent(ST_IDLE);
}

// void Door::ST_Settle(DoorData *) {
//   ESP_LOGD(TAG, "ST_Settle");
// }

void Door::set_relays(bool r1, bool r2) {
  while (!xSemaphoreTake(ignoreFalseTrigger, pdMS_TO_TICKS(100))) {
    ESP_LOGD(TAG, "Waiting for semaphore ignoreFalseTrigger");
  };
  ESP_LOGD(TAG, "Took semaphore ignoreFalseTrigger");
  gpio_set_level(GPIO_NUM_RELAY1, r1 ? 0 : 1);
  gpio_set_level(GPIO_NUM_RELAY2, r2 ? 0 : 1);
  vTaskDelay(pdMS_TO_TICKS(200));
  xSemaphoreGive(ignoreFalseTrigger);
  ESP_LOGD(TAG, "Gave semaphore ignoreFalseTrigger");
}

void Door::start_open() {
  set_relays(true, false);
}

void Door::start_close() {
  set_relays(false, true);
}

void Door::stop() {
  set_relays(false, false);
}
