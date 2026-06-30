#include "logger.h"
#include "../config/config.h"
#include "../sensor/sensor.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

static const char *TAG = "LOGGER";

extern EventGroupHandle_t system_event_group;
#define EVENT_WDT_KICK_LOGGER_BIT (1 << 4)

static TaskHandle_t logger_task_handle = NULL;
static uint32_t queue_dropped_logs = 0;

static void logger_task(void *pvParameters) {
    sensor_reading_t reading;
    
    while (1) {
        // Feed watchdog
        if (system_event_group != NULL) {
            xEventGroupSetBits(system_event_group, EVENT_WDT_KICK_LOGGER_BIT);
        }
        
        // Block on sensor queue. Wake up every 200ms to kick watchdog even if no logs arrive,
        // preventing watchdog trip during quiet times.
        if (xQueueReceive(sensor_queue, &reading, pdMS_TO_TICKS(200)) == pdTRUE) {
            sensor_log_t log;
            log.timestamp = reading.timestamp;
            log.sensor_id = reading.sensor_id;
            log.value = reading.value;
            log.status = reading.status;
            
            if (!storage_add_log(&log)) {
                queue_dropped_logs++;
                ESP_LOGE(TAG, "Failed to write log to storage");
            }
        }
    }
}

void logger_init(void) {
    queue_dropped_logs = 0;
}

void logger_task_start(void) {
    if (logger_task_handle == NULL) {
        xTaskCreate(logger_task, "LoggerTask", STACK_LOGGER_TASK, NULL, PRIO_LOGGER_TASK, &logger_task_handle);
    }
}

uint32_t logger_get_dropped_count(void) {
    storage_stats_t stats;
    storage_get_stats(&stats);
    return stats.dropped_logs + queue_dropped_logs;
}

void logger_reset_dropped_count(void) {
    queue_dropped_logs = 0;
}
