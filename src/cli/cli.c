#include "cli.h"
#include "../config/config.h"
#include "../sensor/sensor.h"
#include "../storage/storage.h"
#include "../logger/logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/usb_serial_jtag.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define MAX_CLI_COMMANDS 32

extern EventGroupHandle_t system_event_group;
#define EVENT_LOGGING_ACTIVE_BIT (1 << 0)
#define EVENT_CLI_ACTIVE_BIT     (1 << 1)
#define EVENT_ERROR_BIT          (1 << 2)
#define EVENT_WDT_KICK_CLI_BIT   (1 << 5)

static cli_command_t cli_commands[MAX_CLI_COMMANDS];
static int cli_command_count = 0;

static SemaphoreHandle_t cli_mutex = NULL;
static StaticSemaphore_t cli_mutex_buffer;

static TaskHandle_t cli_task_handle = NULL;

// Forward declarations of command handlers
static void cli_help_handler(char *args);
static void cli_status_handler(char *args);
static void cli_start_handler(char *args);
static void cli_stop_handler(char *args);
static void cli_clear_handler(char *args);
static void cli_logs_handler(char *args);
static void cli_log_handler(char *args);
static void cli_sensor_handler(char *args);
static void cli_memory_handler(char *args);
static void cli_tasks_handler(char *args);
static void cli_uptime_handler(char *args);
static void cli_reboot_handler(char *args);
static void cli_version_handler(char *args);
static void cli_time_handler(char *args);
static void cli_export_handler(char *args);
static void cli_stats_handler(char *args);

void cli_init(void) {
    if (cli_mutex == NULL) {
        cli_mutex = xSemaphoreCreateMutexStatic(&cli_mutex_buffer);
    }
    cli_command_count = 0;
    
    // Register commands
    cli_register_command("help", "Lists all available commands", cli_help_handler);
    cli_register_command("status", "Displays logger status, rate, active sensors", cli_status_handler);
    cli_register_command("start", "Starts sensor logging to buffer", cli_start_handler);
    cli_register_command("stop", "Stops sensor logging to buffer", cli_stop_handler);
    cli_register_command("clear", "Clears all log records", cli_clear_handler);
    cli_register_command("logs", "Prints all stored sensor logs", cli_logs_handler);
    cli_register_command("log", "Logger subcommands: 'log count'", cli_log_handler);
    cli_register_command("sensor", "Sensor config: 'sensor list|enable <id>|disable <id>|rate <ms>'", cli_sensor_handler);
    cli_register_command("memory", "Displays current free and minimum heap", cli_memory_handler);
    cli_register_command("heap", "Alias for memory", cli_memory_handler);
    cli_register_command("tasks", "Displays list of tasks and state info", cli_tasks_handler);
    cli_register_command("stack", "Alias for tasks", cli_tasks_handler);
    cli_register_command("uptime", "Displays system uptime in seconds", cli_uptime_handler);
    cli_register_command("reboot", "Restarts the microcontroller", cli_reboot_handler);
    cli_register_command("version", "Displays firmware version information", cli_version_handler);
    cli_register_command("time", "Displays system time since boot", cli_time_handler);
    cli_register_command("export", "Exports logs in CSV or JSON: 'export csv|json'", cli_export_handler);
    cli_register_command("stats", "Displays queue usage and performance metrics", cli_stats_handler);
}

bool cli_register_command(const char *command, const char *help, void (*handler)(char *)) {
    if (cli_mutex == NULL) return false;
    
    if (xSemaphoreTake(cli_mutex, portMAX_DELAY) == pdTRUE) {
        if (cli_command_count >= MAX_CLI_COMMANDS) {
            xSemaphoreGive(cli_mutex);
            return false;
        }
        cli_commands[cli_command_count].command = command;
        cli_commands[cli_command_count].help = help;
        cli_commands[cli_command_count].handler = handler;
        cli_command_count++;
        xSemaphoreGive(cli_mutex);
        return true;
    }
    return false;
}

void cli_execute(char *line) {
    // Trim leading spaces
    while (*line == ' ') line++;
    if (*line == '\0') return;
    
    // Find the end of the command name
    char *cmd_end = line;
    while (*cmd_end != ' ' && *cmd_end != '\0') {
        cmd_end++;
    }
    
    char cmd_name[64];
    int name_len = cmd_end - line;
    if (name_len >= sizeof(cmd_name)) {
        printf("Command too long\r\n");
        return;
    }
    strncpy(cmd_name, line, name_len);
    cmd_name[name_len] = '\0';
    
    // Arguments are whatever follows the command name (skipping spaces)
    char *args = cmd_end;
    while (*args == ' ') args++;
    
    // Set CLI Active bit
    if (system_event_group != NULL) {
        xEventGroupSetBits(system_event_group, EVENT_CLI_ACTIVE_BIT);
    }
    
    bool found = false;
    if (xSemaphoreTake(cli_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < cli_command_count; i++) {
            if (strcmp(cli_commands[i].command, cmd_name) == 0) {
                xSemaphoreGive(cli_mutex);
                cli_commands[i].handler(args);
                found = true;
                break;
            }
        }
        if (!found) {
            xSemaphoreGive(cli_mutex);
        }
    }
    
    if (!found) {
        printf("Unknown Command\r\nType help.\r\n");
    }
    
    // Clear CLI Active bit
    if (system_event_group != NULL) {
        xEventGroupClearBits(system_event_group, EVENT_CLI_ACTIVE_BIT);
    }
}

void cli_parse(char *line) {
    cli_execute(line);
}

// Handler Definitions
static void cli_help_handler(char *args) {
    printf("Available Commands:\r\n");
    if (xSemaphoreTake(cli_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < cli_command_count; i++) {
            printf("  %-15s : %s\r\n", cli_commands[i].command, cli_commands[i].help);
        }
        xSemaphoreGive(cli_mutex);
    }
}

static void cli_status_handler(char *args) {
    storage_stats_t s_stats;
    storage_get_stats(&s_stats);
    uint32_t rate = sensor_get_rate();
    int active_sensors = sensor_get_enabled_count();
    
    EventBits_t bits = 0;
    if (system_event_group != NULL) {
        bits = xEventGroupGetBits(system_event_group);
    }
    bool logging_active = (bits & EVENT_LOGGING_ACTIVE_BIT) != 0;
    
    printf("--- System Status ---\r\n");
    printf("Logging:        %s\r\n", logging_active ? "ACTIVE" : "STOPPED");
    printf("Sampling Rate:  %lu ms\r\n", (unsigned long)rate);
    printf("Active Sensors: %d\r\n", active_sensors);
    printf("Logs in Buffer: %lu / %d\r\n", (unsigned long)s_stats.current_count, MAX_LOG_RECORDS);
    printf("Total Written:  %lu\r\n", (unsigned long)s_stats.total_logs_written);
    printf("Dropped Logs:   %lu\r\n", (unsigned long)s_stats.dropped_logs);
}

static void cli_start_handler(char *args) {
    if (system_event_group != NULL) {
        xEventGroupSetBits(system_event_group, EVENT_LOGGING_ACTIVE_BIT);
    }
    printf("Sensor logging started.\r\n");
}

static void cli_stop_handler(char *args) {
    if (system_event_group != NULL) {
        xEventGroupClearBits(system_event_group, EVENT_LOGGING_ACTIVE_BIT);
    }
    printf("Sensor logging stopped.\r\n");
}

static void cli_clear_handler(char *args) {
    storage_clear();
    logger_reset_dropped_count();
    printf("Storage logs cleared.\r\n");
}

static void cli_logs_handler(char *args) {
    storage_stats_t stats;
    storage_get_stats(&stats);
    
    if (stats.current_count == 0) {
        printf("No logs stored.\r\n");
        return;
    }
    
    printf("%-14s | %-9s | %-10s | %-6s\r\n", "Timestamp (ms)", "Sensor ID", "Value", "Status");
    printf("------------------------------------------------\r\n");
    for (int i = 0; i < stats.current_count; i++) {
        sensor_log_t log;
        if (storage_get_log(i, &log)) {
            printf("%-14lu | %-9d | %-10.2f | %-6d\r\n", (unsigned long)log.timestamp, log.sensor_id, log.value, log.status);
        }
    }
}

static void cli_log_handler(char *args) {
    if (strcmp(args, "count") == 0) {
        storage_stats_t stats;
        storage_get_stats(&stats);
        printf("Total logs stored: %lu\r\n", (unsigned long)stats.current_count);
    } else {
        printf("Usage: log count\r\n");
    }
}

static void cli_sensor_handler(char *args) {
    char subcmd[32];
    int read = sscanf(args, "%31s", subcmd);
    if (read <= 0) {
        printf("Usage: sensor [list|enable <id>|disable <id>|rate <ms>]\r\n");
        return;
    }
    
    if (strcmp(subcmd, "list") == 0) {
        char buf[512];
        sensor_print_list(buf, sizeof(buf));
        printf("%s", buf);
    } else if (strcmp(subcmd, "enable") == 0) {
        int sensor_id;
        if (sscanf(args + 6, "%d", &sensor_id) == 1) {
            if (sensor_set_enabled(sensor_id, true)) {
                printf("Sensor ID %d enabled.\r\n", sensor_id);
            } else {
                printf("Sensor ID %d not found.\r\n", sensor_id);
            }
        } else {
            printf("Usage: sensor enable <id>\r\n");
        }
    } else if (strcmp(subcmd, "disable") == 0) {
        int sensor_id;
        if (sscanf(args + 7, "%d", &sensor_id) == 1) {
            if (sensor_set_enabled(sensor_id, false)) {
                printf("Sensor ID %d disabled.\r\n", sensor_id);
            } else {
                printf("Sensor ID %d not found.\r\n", sensor_id);
            }
        } else {
            printf("Usage: sensor disable <id>\r\n");
        }
    } else if (strcmp(subcmd, "rate") == 0) {
        int rate_ms;
        if (sscanf(args + 4, "%d", &rate_ms) == 1 && rate_ms > 0) {
            if (sensor_set_rate(rate_ms)) {
                printf("Sampling rate set to %d ms.\r\n", rate_ms);
            } else {
                printf("Failed to set sampling rate.\r\n");
            }
        } else {
            printf("Usage: sensor rate <ms>\r\n");
        }
    } else {
        printf("Unknown subcommand. Usage: sensor [list|enable <id>|disable <id>|rate <ms>]\r\n");
    }
}

static void cli_memory_handler(char *args) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    printf("Current Free Heap: %u bytes\r\n", (unsigned int)free_heap);
    printf("Minimum Free Heap: %u bytes\r\n", (unsigned int)min_heap);
}

static void cli_tasks_handler(char *args) {
#if configUSE_TRACE_FACILITY
    char *buf = malloc(2048);
    if (buf == NULL) {
        printf("Out of memory to allocate task list buffer\r\n");
        return;
    }
    printf("%-15s %-10s %-8s %-10s %-6s\r\n", "Task Name", "State", "Priority", "Stack", "Num");
    printf("------------------------------------------------------------\r\n");
    vTaskList(buf);
    printf("%s", buf);
    free(buf);
#else
    printf("Task listing is disabled in sdkconfig (configUSE_TRACE_FACILITY is not set).\r\n");
#endif
}

static void cli_uptime_handler(char *args) {
    uint32_t uptime_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    printf("Uptime: %lu ms (%lu seconds)\r\n", (unsigned long)uptime_ms, (unsigned long)(uptime_ms / 1000));
}

static void cli_reboot_handler(char *args) {
    printf("Rebooting system...\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void cli_version_handler(char *args) {
    printf("RTOS Sensor Data Logger v1.0.0 (ESP32-C6 ESP-IDF/PlatformIO)\r\n");
}

static void cli_time_handler(char *args) {
    uint32_t uptime_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    uint32_t hours = uptime_ms / (3600 * 1000);
    uint32_t minutes = (uptime_ms % (3600 * 1000)) / (60 * 1000);
    uint32_t seconds = (uptime_ms % (60 * 1000)) / 1000;
    printf("System Time (since boot): %02lu:%02lu:%02lu\r\n", (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
}

static void cli_export_handler(char *args) {
    storage_stats_t stats;
    storage_get_stats(&stats);
    
    char format[16] = "csv";
    sscanf(args, "%15s", format);
    
    if (strcmp(format, "json") == 0) {
        printf("[\r\n");
        for (int i = 0; i < stats.current_count; i++) {
            sensor_log_t log;
            if (storage_get_log(i, &log)) {
                printf("  {\"ts\": %lu, \"id\": %d, \"val\": %.2f, \"status\": %d}%s\r\n",
                       (unsigned long)log.timestamp,
                       log.sensor_id,
                       log.value,
                       log.status,
                       (i == stats.current_count - 1) ? "" : ",");
            }
        }
        printf("]\r\n");
    } else {
        printf("timestamp,sensor_id,value,status\r\n");
        for (int i = 0; i < stats.current_count; i++) {
            sensor_log_t log;
            if (storage_get_log(i, &log)) {
                printf("%lu,%d,%.2f,%d\r\n", (unsigned long)log.timestamp, log.sensor_id, log.value, log.status);
            }
        }
    }
}

static void cli_stats_handler(char *args) {
    storage_stats_t stats;
    storage_get_stats(&stats);
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap = esp_get_minimum_free_heap_size();
    uint32_t uptime_sec = pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;
    uint32_t dropped = logger_get_dropped_count();
    
    printf("--- Statistics Data ---\r\n");
    printf("Uptime:         %lu seconds\r\n", (unsigned long)uptime_sec);
    printf("Free Heap:      %lu bytes\r\n", (unsigned long)free_heap);
    printf("Minimum Heap:   %lu bytes\r\n", (unsigned long)min_heap);
    printf("Log Count:      %lu / %d\r\n", (unsigned long)stats.current_count, MAX_LOG_RECORDS);
    printf("Dropped Logs:   %lu\r\n", (unsigned long)dropped);
    if (sensor_queue != NULL) {
        printf("Queue Usage:    %d / %d\r\n", uxQueueMessagesWaiting(sensor_queue), LOGGER_QUEUE_LEN);
    }
}

static void cli_task(void *pvParameters) {
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    // Install driver and route stdio to it if not done
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        usb_serial_jtag_driver_install(&cfg);
    }
    usb_serial_jtag_vfs_use_driver();
    
    // Configure line endings for native USB Serial/JTAG console using modern ESP-IDF API
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
#else
    // Configure line endings for VFS UART console using modern ESP-IDF API
    uart_vfs_dev_port_set_rx_line_endings(SYS_UART_PORT, ESP_LINE_ENDINGS_CRLF);
    uart_vfs_dev_port_set_tx_line_endings(SYS_UART_PORT, ESP_LINE_ENDINGS_CRLF);
#endif
    
    // Set non-blocking mode on stdin
    int flags = fcntl(fileno(stdin), F_GETFL, 0);
    fcntl(fileno(stdin), F_SETFL, flags | O_NONBLOCK);
    
    char line_buf[256];
    int line_idx = 0;
    
    printf("\r\n--- RTOS Sensor Data Logger CLI ---\r\n");
    printf("Type 'help' to list commands.\r\n");
    printf("> ");
    fflush(stdout);
    
    while (1) {
        if (system_event_group != NULL) {
            xEventGroupSetBits(system_event_group, EVENT_WDT_KICK_CLI_BIT);
        }
        
        int c = getchar();
        if (c != EOF) {
            if (c == '\r' || c == '\n') {
                line_buf[line_idx] = '\0';
                printf("\r\n");
                if (line_idx > 0) {
                    cli_execute(line_buf);
                }
                line_idx = 0;
                printf("> ");
                fflush(stdout);
            } else if (c == '\b' || c == 0x7F) {
                if (line_idx > 0) {
                    line_idx--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (c >= 32 && c <= 126) {
                if (line_idx < sizeof(line_buf) - 1) {
                    line_buf[line_idx++] = c;
                    putchar(c);
                    fflush(stdout);
                }
            }
        } else {
            // No characters available, sleep for a short duration to yield CPU and feed WDT
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void cli_task_start(void) {
    if (cli_task_handle == NULL) {
        xTaskCreate(cli_task, "CLITask", STACK_CLI_TASK, NULL, PRIO_CLI_TASK, &cli_task_handle);
    }
}
