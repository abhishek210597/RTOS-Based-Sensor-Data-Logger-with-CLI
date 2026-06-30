#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"
#include "sdkconfig.h"

// System Configuration
#define SYS_UART_PORT UART_NUM_0
#define SYS_UART_BAUDRATE 115200

#define STATUS_LED_GPIO GPIO_NUM_8 // ESP32-C6 DevKit standard GPIO LED

// Queue Sizes
#define LOGGER_QUEUE_LEN 50

// Storage Limits
#define MAX_LOG_RECORDS 500

// Sensor Configuration
#define DEFAULT_SAMPLING_RATE_MS 1000
#define MAX_SENSORS 4

// Task Priorities
#define PRIO_CLI_TASK 5
#define PRIO_WATCHDOG_TASK 4
#define PRIO_STATS_TASK 3
#define PRIO_LOGGER_TASK 3
#define PRIO_SENSOR_TASK 3
#define PRIO_LED_TASK 2

// Task Stack Sizes
#define STACK_CLI_TASK 4096
#define STACK_WATCHDOG_TASK 2048
#define STACK_STATS_TASK 3072
#define STACK_LOGGER_TASK 3072
#define STACK_SENSOR_TASK 3072
#define STACK_LED_TASK 2048

#endif // CONFIG_H
