// Just for fun, it might be interesting to see how many times the door opener is activated.
// This stores a counter flash.

// From esp32.com: NVS keeps key-value pairs in a log-based structure, which works well
// for minimizing flash wear. It works best when you use small values, i.e. 8-64 bit
// integers. For blobs and strings, especially large ones (1k and above), performance is
// worse. With integers, you can expect one flash sector to be erased once per every 125
// updates of a value in key-value pair.

#pragma once

#include "int_t.h"

void inc_door_opened_counter();
u32  get_door_opened_counter();
