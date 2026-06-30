#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "config/config.h"
#include "storage/storage.h"
#include "sensor/sensor.h"
#include "logger/logger.h"
#include "cli/cli.h"
#include "led/led.h"
#include "stats/stats.h"
#include "watchdog/watchdog.h"

// Define system event group
EventGroupHandle_t system_event_group = NULL;
static StaticEventGroup_t system_event_group_buffer;

#define EVENT_LOGGING_ACTIVE_BIT (1 << 0)

void app_main(void) {
    ESP_LOGI("MAIN", "Initializing RTOS Sensor Data Logger System...");
    
    // Create Event Group using static memory
    system_event_group = xEventGroupCreateStatic(&system_event_group_buffer);
    
    // Enable logging by default
    xEventGroupSetBits(system_event_group, EVENT_LOGGING_ACTIVE_BIT);
    
    // Initialize modules
    storage_init();
    sensor_init();
    logger_init();
    cli_init();
    led_init();
    stats_init();
    watchdog_init();
    
    // Start tasks
    logger_task_start();
    sensor_task_start();
    cli_task_start();
    led_task_start();
    stats_task_start();
    watchdog_task_start();
    
    ESP_LOGI("MAIN", "All RTOS tasks started successfully.");
}