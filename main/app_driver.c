/*  Temperature Sensor demo implementation using RGB LED and timer

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "freertos/task.h"

#include <sdkconfig.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include "am2302_rmt.h"
#include "led_strip.h"
#include <ultrasonic.h>
#include <app_reset.h>
#include "app_priv.h"
#include "esp_log.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIO on which the power will be set */

static TimerHandle_t sensor_timer;

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10
// GPIO assignment
#define LED_STRIP_BLINK_GPIO  19
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 60
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
// DHT GPIO PIN
#define DHT22_GPIO 11

// Ultrasonic
#define MAX_DISTANCE_CM 500 // 5m max
#define TRIGGER_GPIO 13
#define ECHO_GPIO 14

// Led
#define BLINK_INTERVAL 50   // Interval for blink effect (milliseconds)
#define BREATH_INTERVAL 10  // Interval for breath effect (milliseconds)
#define BREATH_STEP 1       // Step size for breath effect
#define WAVE_SPEED 200         // Speed of the wave effect

static const char *TAG_INIT = "Driver Init";
static const char *TAG_DHT = "DHT22";
static const char *TAG_LED = "LED";
static am2302_handle_t dht_22;
static led_strip_handle_t led_light;
static ultrasonic_sensor_t ultrasonic;
// LED lighting parameters
static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = (int)((float)DEFAULT_SATURATION / 100 * 255);
static uint16_t g_value = (int)((float)DEFAULT_BRIGHTNESS / 100 * 255);
static bool g_power = DEFAULT_POWER;
static bool led_on = true;
static bool increasing_brightness = true;
static uint32_t current_brightness = 0;
static uint32_t wave_phase = 0;
static TickType_t last_time = 0;
static int init_led = 0;
static int led_mode = 0;

// Temperature and humidity parameters
static float temp;
static float humd;
// Ultrasonic
static float distance;


// LED CONFIGURATION
led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_LED, "Error configuring LED strip: %d", err);
        // Handle error appropriately, such as returning NULL or terminating the program
    }
    return led_strip;
}

// DHT22 CONFIGURATION
am2302_handle_t configure_dht(void)
{
    am2302_config_t dht22_config = {
        .gpio_num = DHT22_GPIO,
    };
    am2302_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
    };
    am2302_handle_t DHT_22;
    esp_err_t err = am2302_new_sensor_rmt(&dht22_config, &rmt_config, &DHT_22);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DHT, "Error configuring DHT22 sensor: %d", err);
        // Handle error appropriately, such as returning NULL or terminating the program
    }
    return DHT_22;
}


static void app_sensor_update(TimerHandle_t handle)
{
    ESP_ERROR_CHECK(am2302_read_temp_humi(dht_22, &temp, &humd));
    ESP_ERROR_CHECK(ultrasonic_measure(&ultrasonic, MAX_DISTANCE_CM, &distance));
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(Device, "Temperature"),
            esp_rmaker_float(temp));

    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(Device, "Humidity"),
            esp_rmaker_float(humd));

    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(Device, "Distance"),
            esp_rmaker_float(distance));

    if(temp >= 35.0){
        esp_rmaker_raise_alert("Too Hot!!!");
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
}


static void app_light_set_led(void *pvParameters)
{
    while(1){

        if(g_power){
            TickType_t current_time = xTaskGetTickCount();
            switch (led_mode) {
                case 1:
                    if (current_time - last_time >= BLINK_INTERVAL) {
                        led_on = !led_on;
                        if (led_on) {
                            for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                                led_strip_set_pixel_hsv(led_light, i, g_hue, g_saturation, g_value);
                            }
                            ESP_ERROR_CHECK(led_strip_refresh(led_light));
                        } else {
                            led_strip_clear(led_light);
                        }
                        last_time = current_time;
                    }
                    break;
                case 2:

                    if (current_time - last_time >= BREATH_INTERVAL) {
                        if (increasing_brightness) {
                            current_brightness += BREATH_STEP;
                            if (current_brightness >= g_value) {
                                increasing_brightness = false;
                            }
                        } else {
                            current_brightness -= BREATH_STEP;
                            if (current_brightness <= 0) {
                                increasing_brightness = true;
                            }
                        }
                        for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                            led_strip_set_pixel_hsv(led_light, i, g_hue, g_saturation, current_brightness);
                        }
                        ESP_ERROR_CHECK(led_strip_refresh(led_light));
                        last_time = current_time;
                    }
                    break;
                
                case 3:
                    // Update the wave phase based on the time elapsed
                    if (current_time - last_time >= (1000 / WAVE_SPEED)) {
                        wave_phase+=5;
                        last_time = current_time;
                    }

                    // Update LED color with wave effect
                    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                        // Calculate position along the wave
                        float position = ((float)i / LED_STRIP_LED_NUMBERS) * 360;
                        // Calculate color based on position and phase of the wave
                        uint32_t wave_hue = ((uint32_t)(position + (float)wave_phase)) % 360;
                        led_strip_set_pixel_hsv(led_light, i, wave_hue, g_saturation, g_value);
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_light));
                    break;

                default:
                    // Update LED color normally
                    if (init_led == 0)
                    {
                        led_strip_clear(led_light);
                        init_led++;
                    }
                    for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                        led_strip_set_pixel_hsv(led_light, i, g_hue, g_saturation, g_value);
                    }
                    ESP_ERROR_CHECK(led_strip_refresh(led_light));
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Example delay of 10 milliseconds
    }
    vTaskDelete(NULL);

}

esp_err_t app_light_set_power(bool power)
{
    g_power = power;
    if (power) {
        return g_power;
    } else {
        led_strip_clear(led_light);
    }
    return ESP_OK;
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_value = (int)((float)brightness / 100 * 255);
    return g_value;
}
esp_err_t app_light_set_hue(uint16_t hue)
{
    g_hue = hue;
    return g_hue;
}
esp_err_t app_light_set_saturation(uint16_t saturation)
{
    g_saturation = (int)((float)saturation / 100 * 255);
    return g_saturation;
}

esp_err_t app_light_set_mode(char *mode)
{

    if (strcmp(mode, "Always") == 0) {
        led_mode = 0;
    } 
    else if (strcmp(mode, "Blink") == 0) {
        led_mode = 1;
    }
    else if (strcmp(mode, "Breath") == 0) {
        led_mode = 2;
    }
    else if (strcmp(mode, "Rainbow Wave") == 0) {
        led_mode = 3;
    }
    else {
        led_mode = 0;
    }
    return led_mode;
}

esp_err_t app_sensor_init(void)
{
    led_light = configure_led();
    if (!led_light) {
        ESP_LOGE(TAG_INIT, "LED strip initialization failed");
        return ESP_FAIL;
    }
    dht_22 = configure_dht();
    if (!dht_22) {
        ESP_LOGE(TAG_INIT, "DHT22 sensor initialization failed");
        // Clean up any resources allocated for LED strip before returning failure
        led_strip_del(led_light);
        return ESP_FAIL;
    }

    ultrasonic = (ultrasonic_sensor_t){
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO,
    };
    esp_err_t err = ultrasonic_init(&ultrasonic);
    if (err != ESP_OK)
    {
        ESP_LOGE("SENSOR", "Ultrasonic sensor initialization failed");
        return err;
    }

    xTaskCreatePinnedToCore(app_light_set_led, "led_task", 8192, NULL, 1, NULL, 1);
    sensor_timer = xTimerCreate("app_sensor_update_tm", (REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, app_sensor_update);
    if (sensor_timer) {
        xTimerStart(sensor_timer, 0);
    }
    return ESP_FAIL;
    if (sensor_timer == NULL) {
        ESP_LOGE(TAG_INIT, "Error creating sensor update timer");
        // Clean up resources allocated for LED strip and DHT22 sensor before returning failure
        led_strip_del(led_light);
        am2302_del_sensor(dht_22);
        return ESP_FAIL;
    }

    if (xTimerStart(sensor_timer, 0) != pdPASS) {
        ESP_LOGE(TAG_INIT, "Error starting sensor update timer");
        // Clean up resources allocated for LED strip, DHT22 sensor, and timer before returning failure
        led_strip_del(led_light);
        am2302_del_sensor(dht_22);
        xTimerDelete(sensor_timer, 0);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_INIT, "Sensor initialization completed successfully");
    return ESP_OK;
}

void app_driver_init()
{
    app_sensor_init();
    app_reset_button_register(app_reset_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL),
                WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
}
