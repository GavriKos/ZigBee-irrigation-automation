#include "esp_zigbee_core.h"


#define ZB_ED_ROLE

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_ESP_LIGHT_ENDPOINT           10      /* esp light bulb device endpoint, used to process light controlling commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */
#define SENSOR_ENDPOINT 1

#define MANUFACTURER_NAME "Gavrikos"
#define MODEL_NAME "Irrigator 1.0"
#define FIRMWARE_VERSION "1.0"

#define ENGINE_PIN GPIO_NUM_5
#define SOIL_HUMIDITY_POWER_PIN GPIO_NUM_1
#define SOIL_HUMIDITY_PIN ADC1_CHANNEL_3
#define SOIL_HUMIDITY_POWER_INTERVAL 1000
#define MEATHURE_INTERVAL 60000
#define ZIGBEE_ATTRIBUTE_UPDATE_INTERVAL 5000
#define SOIL_HUMIDITY_ZIGBEE_MIN 2500
#define SOIL_HUMIDITY_ZIGBEE_MAX 3500
#define SOIL_HUMIDITY_PIN_MAX 3000.0

#define HARD_RESET_PIN 0

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = RADIO_MODE_NATIVE,                        \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,      \
    }


uint16_t soilHumidityValue = 0;
static const char *TAG = "device";
static bool isZigBeeConnected = false;
static char manufacturer[16], model[16], firmware_version[16];