#include <stdio.h>
#include <stdlib.h>
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "main.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

/********************* Define functions **************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static void set_zcl_string(char *buffer, char *value)
{
    buffer[0] = (char)strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

void update_attribute()
{
    if (isZigBeeConnected)
    {
        esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT,
                                                                     ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
                                                                     ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &soilHumidityValue, false);

        /* Check for error */
        if (state_tmp != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Setting temperature attribute failed!");
        }

        esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
        report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
        report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID;
        report_attr_cmd.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
        report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT;
        report_attr_cmd.zcl_basic_cmd.src_endpoint = SENSOR_ENDPOINT;

        esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);

        ESP_LOGI(TAG, "Update ZigBee attributes. soilHumidityValue: %d", (int)soilHumidityValue);
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new())
            {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            else
            {
                ESP_LOGI(TAG, "Device rebooted");
                isZigBeeConnected = true;
            }
        }
        else
        {
            /* commissioning failed */
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            isZigBeeConnected = true;
        }
        else
        {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            vTaskDelay(1000 / portTICK_PERIOD_MS); // добавьте задержку перед повторной попыткой подключения
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        break;
    case ESP_ZB_ZDO_DEVICE_UNAVAILABLE:
        esp_err_t err = esp_zb_start(false);
        if (err != ESP_OK)
        {
            printf("Ошибка при попытке переподключения: %d\n", err);
        }
        break;
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static void MesureWhileEngineOn()
{
    gpio_set_level(SOIL_HUMIDITY_POWER_PIN, 1);
    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(SOIL_HUMIDITY_PIN, ADC_ATTEN_DB_12);
    int oneDelayCoeff = 20;
    int loop = ENGINE_TIMER / portTICK_PERIOD_MS/10; //i dont know wtf 10 here (((
    loop = loop/oneDelayCoeff;
    while (loop > 0)
    {
        float adc_value = adc1_get_raw(ADC1_CHANNEL_3);
        if (adc_value > 0)
        {
            uint16_t mesuredSoilHumidityValue = (100 - (((adc_value - 1100) / (2500 - 1100)) * 100)) * 100;
            if (mesuredSoilHumidityValue < 10000 && mesuredSoilHumidityValue > 0)
            {
                soilHumidityValue = mesuredSoilHumidityValue;
            }
        }
        update_attribute();
        ESP_LOGI(TAG, "Measure soil humidity. Adc value: %0.2f, value: %d", adc_value, (int)soilHumidityValue);
        loop-=1;
        vTaskDelay(portTICK_PERIOD_MS*oneDelayCoeff);
    }

    gpio_set_level(SOIL_HUMIDITY_POWER_PIN, 0);
    vTaskDelete(NULL);
}

static void Engine_stop()
{
    vTaskDelay(ENGINE_TIMER / portTICK_PERIOD_MS);
    int engine_state = 0;
    ESP_LOGI(TAG, "Engine sets to %s", engine_state ? "On" : "Off");
    gpio_set_level(ENGINE_PIN, engine_state ? 1 : 0);

    if (isZigBeeConnected)
    {
        uint8_t onOff = 0;

        esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &onOff, false);
    }

    vTaskDelete(NULL);
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool engine_state = 0;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    bool isProceed = false;

    if (message->info.dst_endpoint == SENSOR_ENDPOINT)
    {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF)
        {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL)
            {
                engine_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : engine_state;
                ESP_LOGI(TAG, "Engine sets to %s", engine_state ? "On" : "Off");
                gpio_set_level(ENGINE_PIN, engine_state ? 1 : 0);
                if (engine_state == true)
                {
                    xTaskCreate(Engine_stop, "Engine_stop", 4096, NULL, 5, NULL);
                    xTaskCreate(MesureWhileEngineOn, "MesureWhileEngineOn", 4096, NULL, 5, NULL);
                }
                isProceed = true;
            }
        }
    }

    if (isProceed == false)
    {
        ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
                 message->attribute.id, message->attribute.data.size);
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        break;
    default:
        ESP_LOGI(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters)
{
    uint16_t undefined_value;
    undefined_value = 0x8000;

    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    set_zcl_string(manufacturer, MANUFACTURER_NAME);
    set_zcl_string(model, MODEL_NAME);
    set_zcl_string(firmware_version, FIRMWARE_VERSION);
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, firmware_version);

    esp_zb_attribute_list_t *esp_zb_humidity_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &undefined_value);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID, &undefined_value);

    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_humidity_meas_cluster(esp_zb_cluster_list, esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    /* on-off cluster create with standard cluster config*/
    // -----------------------------------------------------

    esp_zb_attribute_list_t *attr_list_on_off = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);

    esp_zb_on_off_cluster_add_attr(attr_list_on_off, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &undefined_value);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, attr_list_on_off, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, SENSOR_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID);

    /* END */
    esp_zb_device_register(esp_zb_ep_list);

    esp_zb_core_action_handler_register(zb_action_handler);

    esp_zb_zcl_reporting_info_t reporting_info =
        {
            .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
            .ep = SENSOR_ENDPOINT,
            .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
            .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
            .u.send_info.min_interval = 1,
            .u.send_info.max_interval = 0,
            .u.send_info.def_min_interval = 1,
            .u.send_info.def_max_interval = 0,
            .u.send_info.delta.u16 = 100,
            .attr_id = ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
        };

    esp_zb_zcl_update_reporting_info(&reporting_info);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void update_gpio_inputs()
{
    while (1)
    {
        gpio_set_level(SOIL_HUMIDITY_POWER_PIN, 1);
        adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
        adc1_config_channel_atten(SOIL_HUMIDITY_PIN, ADC_ATTEN_DB_12);
        vTaskDelay(SOIL_HUMIDITY_POWER_INTERVAL / portTICK_PERIOD_MS);

        float adc_value = adc1_get_raw(ADC1_CHANNEL_3);
        if (adc_value > 0)
        {
            uint16_t mesuredSoilHumidityValue = (100 - (((adc_value - 1100) / (2500 - 1100)) * 100)) * 100;
            if (mesuredSoilHumidityValue < 10000 && mesuredSoilHumidityValue > 0)
            {
                soilHumidityValue = mesuredSoilHumidityValue;
            }
        }
        ESP_LOGI(TAG, "Measure soil humidity. Adc value: %0.2f, value: %d", adc_value, (int)soilHumidityValue);

        gpio_set_level(SOIL_HUMIDITY_POWER_PIN, 0);

        vTaskDelay(MEATHURE_INTERVAL / portTICK_PERIOD_MS);
    }
}

void updateHardresetTask()
{
    while (1)
    {
        int value = gpio_get_level(HARD_RESET_PIN);
        if (value == 0)
        {
            ESP_LOGW(TAG, "HARD RESET PRESSED");
            esp_zb_factory_reset();
        }

        vTaskDelay(MEATHURE_INTERVAL / portTICK_PERIOD_MS);
    }
}

void updateReboot()
{
    vTaskDelay(REBOOT_TIMER / portTICK_PERIOD_MS);
    esp_restart();
}

void update_attribute_task()
{
    while (1)
    {
        update_attribute();
        vTaskDelay(ZIGBEE_ATTRIBUTE_UPDATE_INTERVAL / portTICK_PERIOD_MS);
    }
}

void preparePins()
{
    gpio_reset_pin(ENGINE_PIN);
    gpio_set_direction(ENGINE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ENGINE_PIN, 0);

    gpio_reset_pin(SOIL_HUMIDITY_POWER_PIN);
    gpio_set_direction(SOIL_HUMIDITY_POWER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SOIL_HUMIDITY_POWER_PIN, 0);

    gpio_reset_pin(HARD_RESET_PIN);
    gpio_set_direction(HARD_RESET_PIN, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(SOIL_HUMIDITY_PIN, ADC_ATTEN_DB_11);
}

void app_main(void)
{
    preparePins();

    ESP_LOGW("REBOOT!: ", "REBOOT REASON: %d", esp_reset_reason());

    esp_zb_platform_config_t config =
        {
            .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
        };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(update_attribute_task, "Update_attribute_value", 4096, NULL, 5, NULL);
    xTaskCreate(update_gpio_inputs, "update_gpio_inputs", 4096, NULL, 5, NULL);
    xTaskCreate(updateHardresetTask, "updateHardreset", 4096, NULL, 5, NULL);
    xTaskCreate(updateReboot, "updateReboot", 4096, NULL, 5, NULL);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
