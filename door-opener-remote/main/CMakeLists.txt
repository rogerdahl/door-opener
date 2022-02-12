set(COMPONENT_REQUIRES bt)

idf_component_register(
    SRC_DIRS
    "."

    INCLUDE_DIRS
    "."

    REQUIRES
    "bt" # esp_bt.h
    "console" # esp_console.h
    "m5stickc-idf" # m5stickc.h
    # "esp_ble"
    # Hangs when enabled?
    # "esp_hid"
    # "esp_bt_device",
)
