#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Sensor reading structure sent to queue
typedef struct {
    uint32_t timestamp;
    uint8_t sensor_id;
    char name[16];
    float value;
    uint8_t status;
} sensor_reading_t;

// Sensor queue handle
extern QueueHandle_t sensor_queue;

// Initialize the sensors
void sensor_init(void);

// Start the sensor sampling task
void sensor_task_start(void);

// Enable/Disable a specific sensor by ID
bool sensor_set_enabled(uint8_t sensor_id, bool enabled);

// Set the global sampling rate
bool sensor_set_rate(uint32_t rate_ms);

// Get the current sampling rate
uint32_t sensor_get_rate(void);

// Print the list of sensors to buffer/stdout
void sensor_print_list(char *dest_buf, size_t max_len);

// Get the count of enabled sensors
int sensor_get_enabled_count(void);

#endif // SENSOR_H
