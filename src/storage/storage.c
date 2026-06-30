#include "storage.h"
#include "../config/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static sensor_log_t log_buffer[MAX_LOG_RECORDS];
static int head = 0;
static int tail = 0;
static int count = 0;
static uint32_t total_written = 0;
static uint32_t dropped = 0;

static SemaphoreHandle_t storage_mutex = NULL;
static StaticSemaphore_t storage_mutex_buffer;

void storage_init(void) {
    if (storage_mutex == NULL) {
        storage_mutex = xSemaphoreCreateMutexStatic(&storage_mutex_buffer);
    }
    head = 0;
    tail = 0;
    count = 0;
    total_written = 0;
    dropped = 0;
}

bool storage_add_log(const sensor_log_t *log) {
    if (storage_mutex == NULL) return false;
    
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    log_buffer[head] = *log;
    head = (head + 1) % MAX_LOG_RECORDS;
    total_written++;
    
    if (count < MAX_LOG_RECORDS) {
        count++;
    } else {
        // Overflow: buffer is full, overwrite the oldest record at tail
        tail = (tail + 1) % MAX_LOG_RECORDS;
        dropped++;
    }
    
    xSemaphoreGive(storage_mutex);
    return true;
}

bool storage_get_log(int index, sensor_log_t *log_out) {
    if (storage_mutex == NULL || log_out == NULL) return false;
    
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    if (index < 0 || index >= count) {
        xSemaphoreGive(storage_mutex);
        return false;
    }
    
    // index 0 is oldest (at tail)
    int actual_idx = (tail + index) % MAX_LOG_RECORDS;
    *log_out = log_buffer[actual_idx];
    
    xSemaphoreGive(storage_mutex);
    return true;
}

void storage_get_stats(storage_stats_t *stats_out) {
    if (stats_out == NULL) return;
    if (storage_mutex == NULL) {
        stats_out->total_logs_written = 0;
        stats_out->dropped_logs = 0;
        stats_out->current_count = 0;
        return;
    }
    
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        stats_out->total_logs_written = total_written;
        stats_out->dropped_logs = dropped;
        stats_out->current_count = count;
        xSemaphoreGive(storage_mutex);
    }
}

void storage_clear(void) {
    if (storage_mutex == NULL) return;
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        head = 0;
        tail = 0;
        count = 0;
        total_written = 0;
        dropped = 0;
        xSemaphoreGive(storage_mutex);
    }
}
