#include "sensor.h"
#include "../config/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define NUM_SENSORS 4

// Event bits defined in main or system header
extern EventGroupHandle_t system_event_group;
#define EVENT_LOGGING_ACTIVE_BIT (1 << 0)
#define EVENT_WDT_KICK_SENSOR_BIT (1 << 3)

typedef struct {
    uint8_t id;
    char name[16];
    bool enabled;
    float last_value;
    uint8_t status;
} sensor_state_t;

static sensor_state_t sensors[NUM_SENSORS] = {
    {1, "Temp", true, 28.5f, 0},
    {2, "ADC_Volt", true, 1.65f, 0},
    {3, "Light_LDR", true, 50.0f, 0},
    {4, "BMP_Press", true, 1013.25f, 0}
};

static uint32_t sampling_rate_ms = DEFAULT_SAMPLING_RATE_MS;

// Queue and Mutex static allocation
QueueHandle_t sensor_queue = NULL;
static StaticQueue_t sensor_queue_buffer;
static uint8_t sensor_queue_storage[LOGGER_QUEUE_LEN * sizeof(sensor_reading_t)];

static SemaphoreHandle_t sensor_mutex = NULL;
static StaticSemaphore_t sensor_mutex_buffer;

static TaskHandle_t sensor_task_handle = NULL;

static float generate_dummy_value(uint8_t id) {
    uint64_t uptime_us = esp_timer_get_time();
    double time_sec = (double)uptime_us / 1000000.0;
    
    switch (id) {
        case 1: // Temperature (25-45C)
            return 35.0f + 10.0f * (float)sin(time_sec / 60.0);
        case 2: // ADC Volt (0-3.3V)
            return 1.65f + 1.65f * (float)sin(time_sec / 10.0);
        case 3: // Light LDR (0-100%)
            return 50.0f + 40.0f * (float)sin(time_sec / 300.0) + ((float)(rand() % 10) - 5.0f) * 0.1f;
        case 4: // BMP pressure (950-1050 hPa)
            return 1000.0f + 50.0f * (float)cos(time_sec / 120.0);
        default:
            return 0.0f;
    }
}

static void sensor_task(void *pvParameters) {
    while (1) {
        uint32_t current_rate_ms;
        
        // Read active sampling rate
        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) {
            current_rate_ms = sampling_rate_ms;
            xSemaphoreGive(sensor_mutex);
        } else {
            current_rate_ms = DEFAULT_SAMPLING_RATE_MS;
        }
        
        // Sleep in 500ms increments to keep feeding the watchdog
        uint32_t elapsed_ms = 0;
        while (elapsed_ms < current_rate_ms) {
            if (system_event_group != NULL) {
                xEventGroupSetBits(system_event_group, EVENT_WDT_KICK_SENSOR_BIT);
            }
            uint32_t step = 500;
            if (current_rate_ms - elapsed_ms < step) {
                step = current_rate_ms - elapsed_ms;
            }
            vTaskDelay(pdMS_TO_TICKS(step));
            elapsed_ms += step;
            
            // Read active sampling rate again in case it changes during long sleep
            if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                current_rate_ms = sampling_rate_ms;
                xSemaphoreGive(sensor_mutex);
            }
        }
        
        // Only log if logging is active
        if (system_event_group != NULL) {
            EventBits_t bits = xEventGroupGetBits(system_event_group);
            if (!(bits & EVENT_LOGGING_ACTIVE_BIT)) {
                continue; // logging is stopped
            }
        }
        
        // Read and queue enabled sensors
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            uint32_t timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
            
            for (int i = 0; i < NUM_SENSORS; i++) {
                if (sensors[i].enabled) {
                    sensors[i].last_value = generate_dummy_value(sensors[i].id);
                    
                    sensor_reading_t reading;
                    reading.timestamp = timestamp;
                    reading.sensor_id = sensors[i].id;
                    strncpy(reading.name, sensors[i].name, sizeof(reading.name) - 1);
                    reading.name[sizeof(reading.name) - 1] = '\0';
                    reading.value = sensors[i].last_value;
                    reading.status = sensors[i].status;
                    
                    // Send to queue. If queue is full, it drops the packet.
                    // Logger task will track queue capacity/dropped logs at the queue receiver end.
                    xQueueSend(sensor_queue, &reading, 0);
                }
            }
            xSemaphoreGive(sensor_mutex);
        }
    }
}

void sensor_init(void) {
    if (sensor_mutex == NULL) {
        sensor_mutex = xSemaphoreCreateMutexStatic(&sensor_mutex_buffer);
    }
    if (sensor_queue == NULL) {
        sensor_queue = xQueueCreateStatic(LOGGER_QUEUE_LEN, sizeof(sensor_reading_t), sensor_queue_storage, &sensor_queue_buffer);
    }
}

void sensor_task_start(void) {
    if (sensor_task_handle == NULL) {
        xTaskCreate(sensor_task, "SensorTask", STACK_SENSOR_TASK, NULL, PRIO_SENSOR_TASK, &sensor_task_handle);
    }
}

bool sensor_set_enabled(uint8_t sensor_id, bool enabled) {
    if (sensor_mutex == NULL) return false;
    
    bool found = false;
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (sensors[i].id == sensor_id) {
                sensors[i].enabled = enabled;
                found = true;
                break;
            }
        }
        xSemaphoreGive(sensor_mutex);
    }
    return found;
}

bool sensor_set_rate(uint32_t rate_ms) {
    if (sensor_mutex == NULL || rate_ms == 0) return false;
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sampling_rate_ms = rate_ms;
        xSemaphoreGive(sensor_mutex);
        return true;
    }
    return false;
}

uint32_t sensor_get_rate(void) {
    uint32_t rate = DEFAULT_SAMPLING_RATE_MS;
    if (sensor_mutex != NULL) {
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            rate = sampling_rate_ms;
            xSemaphoreGive(sensor_mutex);
        }
    }
    return rate;
}

void sensor_print_list(char *dest_buf, size_t max_len) {
    if (dest_buf == NULL || max_len == 0) return;
    dest_buf[0] = '\0';
    
    if (sensor_mutex == NULL) return;
    
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        size_t written = 0;
        written += snprintf(dest_buf + written, max_len - written, "%-5s | %-12s | %-8s | %-10s | %-6s\r\n", "ID", "Name", "Enabled", "Last Val", "Status");
        written += snprintf(dest_buf + written, max_len - written, "---------------------------------------------------------\r\n");
        
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (written >= max_len) break;
            written += snprintf(dest_buf + written, max_len - written, "%-5d | %-12s | %-8s | %-10.2f | %-6d\r\n",
                                sensors[i].id,
                                sensors[i].name,
                                sensors[i].enabled ? "YES" : "NO",
                                sensors[i].last_value,
                                sensors[i].status);
        }
        xSemaphoreGive(sensor_mutex);
    }
}

int sensor_get_enabled_count(void) {
    int count = 0;
    if (sensor_mutex != NULL) {
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (int i = 0; i < NUM_SENSORS; i++) {
                if (sensors[i].enabled) {
                    count++;
                }
            }
            xSemaphoreGive(sensor_mutex);
        }
    }
    return count;
}
