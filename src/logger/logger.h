#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

// Initialize the logger task
void logger_init(void);

// Start the logger task
void logger_task_start(void);

// Get number of dropped log packets
uint32_t logger_get_dropped_count(void);

// Reset dropped log count
void logger_reset_dropped_count(void);

#endif // LOGGER_H
