#include "led.h"
#include "../config/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

extern EventGroupHandle_t system_event_group;
#define EVENT_LOGGING_ACTIVE_BIT (1 << 0)
#define EVENT_CLI_ACTIVE_BIT     (1 << 1)
#define EVENT_ERROR_BIT           (1 << 2)

static TaskHandle_t led_task_handle = NULL;

void led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(STATUS_LED_GPIO, 0);
}

static void led_task(void *pvParameters) {
    while (1) {
        EventBits_t bits = 0;
        if (system_event_group != NULL) {
            bits = xEventGroupGetBits(system_event_group);
        }
        
        if (bits & EVENT_ERROR_BIT) {
            // Double Blink: Blink twice, then wait
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(700));
        } else if (bits & EVENT_CLI_ACTIVE_BIT) {
            // Solid ON
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (bits & EVENT_LOGGING_ACTIVE_BIT) {
            // Fast Blink: 100ms ON, 100ms OFF
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            // Slow Blink: 500ms ON, 500ms OFF
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void led_task_start(void) {
    if (led_task_handle == NULL) {
        xTaskCreate(led_task, "LEDTask", STACK_LED_TASK, NULL, PRIO_LED_TASK, &led_task_handle);
    }
}
