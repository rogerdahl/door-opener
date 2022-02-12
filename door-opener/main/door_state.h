#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "state_machine.h"

#include "int_t.h"

// Wait while door is opening.
static const int OPENING_DURATION_MS = 11000;
// Wait with the door open.
static const int OPEN_DURATION_MS = 20000;
// By always closing a bit longer than we open, we ensure that the actuator is
// maximally extended in the closed position.
static const int CLOSING_DURATION_MS = OPENING_DURATION_MS + 200;

// Event data passed into state machine.
struct DoorData : public EventData {
  // int speed;
};

class Door : public StateMachine {
public:
  Door(QueueHandle_t queueHandle);
  void Open();
  void Next();

private:
  // State machine state functions
  // For functions that don't need event data, declare as taking the base, EventData*.
  void ST_Idle(DoorData*);
  void ST_Opening(DoorData*);
  void ST_Open(DoorData*);
  void ST_Closing(DoorData*);
  void ST_Closed(DoorData*);
  // void ST_Settle(DoorData *);

  void set_relays(bool r1, bool r2);
  void start_open();
  void start_close();
  void stop();

  // State map to define state function order
  BEGIN_STATE_MAP
  STATE_MAP_ENTRY(&Door::ST_Idle)
  STATE_MAP_ENTRY(&Door::ST_Opening)
  STATE_MAP_ENTRY(&Door::ST_Open)
  STATE_MAP_ENTRY(&Door::ST_Closing)
  STATE_MAP_ENTRY(&Door::ST_Closed)
  // STATE_MAP_ENTRY(&Door::ST_Settle)
  END_STATE_MAP

  // State enumeration order must match the order of state method entries in the
  // state map
  enum E_States { ST_IDLE = 0, ST_OPENING, ST_OPEN, ST_CLOSING, ST_CLOSED, ST_MAX_STATES };

  QueueHandle_t queueHandle;
};
