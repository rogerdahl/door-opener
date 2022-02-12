#pragma once

#include "esp_event.h"
#include "freertos/timers.h"

#include "int_t.h"

enum Cmd {
  CMD_NONE,
  CMD_OPEN,
  CMD_NEXT,
};

struct CmdAndArg;

// QueueHandle_t queueHandle;
// TimerHandle_t timerHandle;

void init_state_machine();
void door_state_task(__attribute__((unused)) void* params);
void timer_callback(TimerHandle_t xTimerHandle);
void trigger_open();
void enqueue(const CmdAndArg& ev);
void schedule_next(u32 delay_ms);
void cancel_next();
