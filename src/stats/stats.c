#include "stats.h"
#include "../config/config.h"
#include "../storage/storage.h"
#include "../logger/logger.h"
#include "../sensor/sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include <stdio.h>
#include <stdlib.h>

extern EventGroupHandle_t system_event_group;
#define EVENT_WDT_KICK_STATS_BIT (1 << 6)

static TaskHandle_t stats_task_handle = NULL;

static void stats_task(void *pvParameters) {
    int tick_count = 0;
    
    while (1) {
        // Feed watchdog
        if (system_event_group != NULL) {
            xEventGroupSetBits(system_event_group, EVENT_WDT_KICK_STATS_BIT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        tick_count++;
        
        if (tick_count < 10) {
            continue;
        }
        tick_count = 0;
        
        storage_stats_t storage_stats;
        storage_get_stats(&storage_stats);
        
        size_t free_heap = esp_get_free_heap_size();
        size_t min_heap = esp_get_minimum_free_heap_size();
        uint32_t uptime_sec = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
        uint32_t dropped = logger_get_dropped_count();
        int active_sensors = sensor_get_enabled_count();
        
        printf("\r\n--- 10-Second System Statistics Report ---\r\n");
        printf("System Uptime:  %lu seconds\r\n", uptime_sec);
        printf("Free Heap:      %u bytes\r\n", (unsigned int)free_heap);
        printf("Minimum Heap:   %u bytes\r\n", (unsigned int)min_heap);
        printf("Active Sensors: %d\r\n", active_sensors);
        printf("Log Records:    %lu / %d\r\n", storage_stats.current_count, MAX_LOG_RECORDS);
        printf("Dropped Logs:   %lu\r\n", dropped);
        printf("Queue Usage:    %d / %d\r\n", uxQueueMessagesWaiting(sensor_queue), LOGGER_QUEUE_LEN);
        
#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS
        char *stats_buf = malloc(1024);
        if (stats_buf != NULL) {
            printf("\r\n--- Task Runtime Statistics ---\r\n");
            vTaskGetRunTimeStats(stats_buf);
            printf("%s", stats_buf);
            free(stats_buf);
        }
#endif
        printf("-------------------------------------------\r\n> ");
        fflush(stdout);
    }
}

void stats_init(void) {
    // Initializer
}

void stats_task_start(void) {
    if (stats_task_handle == NULL) {
        xTaskCreate(stats_task, "StatsTask", STACK_STATS_TASK, NULL, PRIO_STATS_TASK, &stats_task_handle);
    }
}
