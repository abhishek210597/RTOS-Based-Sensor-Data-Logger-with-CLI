#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// Sensor log structure
typedef struct
{
    uint32_t timestamp; // Uptime in ms
    uint8_t sensor_id;
    float value;
    uint8_t status;
} sensor_log_t;

// Storage statistics
typedef struct {
    uint32_t total_logs_written;
    uint32_t dropped_logs;
    uint32_t current_count;
} storage_stats_t;

// Initialize storage layer
void storage_init(void);

// Add a log record to storage. Thread-safe.
bool storage_add_log(const sensor_log_t *log);

// Read a log record by index. Thread-safe.
// index is from 0 to current_count - 1 (0 is oldest, count-1 is newest)
bool storage_get_log(int index, sensor_log_t *log_out);

// Get current stats. Thread-safe.
void storage_get_stats(storage_stats_t *stats_out);

// Clear all logs in storage. Thread-safe.
void storage_clear(void);

#endif // STORAGE_H
