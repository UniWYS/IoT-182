#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "cjson.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"


#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_wifi.h>
#include <app_insights.h>

#include "app_priv.h"



esp_rmaker_device_t *Device;

static const char *TAG_RM = "Rain_maker";
static const char *TAG = "app_main";
static const char *valid_strs[] = {"Always","Blink","Breath", "Rainbow Wave"}; 

/* Callback to handle param updates received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, "power") == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_light_set_power(val.val.b);
    } 
    else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
    }
    else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_light_set_hue(val.val.i);
    } 
    else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_light_set_saturation(val.val.i);
    }
    else if (strcmp(param_name, "LED Mode") == 0) {
        ESP_LOGI("haha", "Received value = %s for %s - %s",
                val.val.s, device_name, param_name);
        app_light_set_mode(val.val.s);
    }
    else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

void app_main(void)
{
    app_driver_init();
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };

     /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_wifi_init();
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP Rainmaker Device", "Trial");
    if(!node){
        ESP_LOGE(TAG_RM, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }


    // Create a category
    Device = esp_rmaker_device_create("Node", NULL, NULL);
    esp_rmaker_device_add_cb(Device, write_cb, NULL);

    // Create data structure, naming and add
    esp_rmaker_param_t *temp_param = esp_rmaker_temperature_param_create("Temperature", DEFAULT_TEMPERATURE);
    esp_rmaker_device_add_param(Device, temp_param);
    esp_rmaker_param_t *humd_param = esp_rmaker_temperature_param_create("Humidity", DEFAULT_HUMIDITY);
    esp_rmaker_device_add_param(Device, humd_param);
    esp_rmaker_param_t *dist_param = esp_rmaker_param_create("Distance/m", NULL, esp_rmaker_float(DEFAULT_DISTANCE), PROP_FLAG_READ);
    esp_rmaker_device_add_param(Device, dist_param);
    esp_rmaker_param_t *power_param = esp_rmaker_power_param_create("light power", DEFAULT_POWER);
    esp_rmaker_device_add_param(Device, power_param);
    esp_rmaker_param_t *brightness_param = esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS);
    esp_rmaker_device_add_param(Device, brightness_param);
    esp_rmaker_param_t *hue_param = esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE);
    esp_rmaker_device_add_param(Device, hue_param);
    esp_rmaker_param_t *saturation_param = esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION);
    esp_rmaker_device_add_param(Device, saturation_param);
    esp_rmaker_param_t *mode_param = esp_rmaker_param_create("LED Mode", NULL, esp_rmaker_str(DEFAULT_LED_MODE), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(mode_param, ESP_RMAKER_UI_DROPDOWN);
    esp_rmaker_param_add_valid_str_list(mode_param, valid_strs,4);
    esp_rmaker_device_add_param(Device, mode_param);

    esp_rmaker_device_assign_primary_param(Device, power_param);
    
    esp_rmaker_device_add_attribute(Device, "Serial Number", "012345");
    esp_rmaker_device_add_attribute(Device, "MAC", "xx:yy:zz:aa:bb:cc");
    esp_rmaker_node_add_device(node, Device);




    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    esp_rmaker_start();
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK)
    {
        ESP_LOGE("wifi", "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
}
