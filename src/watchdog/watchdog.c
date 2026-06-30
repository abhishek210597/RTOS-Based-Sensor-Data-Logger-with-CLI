#include "watchdog.h"
#include "../config/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdio.h>

extern EventGroupHandle_t system_event_group;
#define EVENT_LOGGING_ACTIVE_BIT (1 << 0)
#define EVENT_CLI_ACTIVE_BIT     (1 << 1)
#define EVENT_ERROR_BIT           (1 << 2)

#define EVENT_WDT_KICK_SENSOR_BIT (1 << 3)
#define EVENT_WDT_KICK_LOGGER_BIT (1 << 4)
#define EVENT_WDT_KICK_CLI_BIT   (1 << 5)
#define EVENT_WDT_KICK_STATS_BIT (1 << 6)

#define ALL_WDT_KICKS (EVENT_WDT_KICK_SENSOR_BIT | EVENT_WDT_KICK_LOGGER_BIT | EVENT_WDT_KICK_CLI_BIT | EVENT_WDT_KICK_STATS_BIT)

static TaskHandle_t watchdog_task_handle = NULL;

static void watchdog_task(void *pvParameters) {
    // Give tasks some time to start up
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        // Wait for all task check-in bits to be set. Timeout is 15 seconds.
        // It clears the bits upon exit (pdTRUE) and requires all bits (pdTRUE).
        EventBits_t result = xEventGroupWaitBits(
            system_event_group,
            ALL_WDT_KICKS,
            pdTRUE,       // Clear bits on exit
            pdTRUE,       // Wait for all bits
            pdMS_TO_TICKS(15000)
        );
        
        // If the result doesn't have all bits set, we timed out
        if ((result & ALL_WDT_KICKS) != ALL_WDT_KICKS) {
            printf("\r\n[WATCHDOG WARNING] Task stagnation detected! Missing check-ins:\r\n");
            
            bool error_found = false;
            if (!(result & EVENT_WDT_KICK_SENSOR_BIT)) {
                printf("  - Sensor Task (Stuck or Starved)\r\n");
                error_found = true;
            }
            if (!(result & EVENT_WDT_KICK_LOGGER_BIT)) {
                printf("  - Logger Task (Stuck or Starved)\r\n");
                error_found = true;
            }
            if (!(result & EVENT_WDT_KICK_CLI_BIT)) {
                printf("  - CLI Task (Stuck or Starved)\r\n");
                error_found = true;
            }
            if (!(result & EVENT_WDT_KICK_STATS_BIT)) {
                printf("  - Statistics Task (Stuck or Starved)\r\n");
                error_found = true;
            }
            
            if (error_found) {
                // Set system error bit to trigger double blink on status LED
                xEventGroupSetBits(system_event_group, EVENT_ERROR_BIT);
            }
            printf("> ");
            fflush(stdout);
        } else {
            // All tasks healthy, make sure error bit is cleared
            xEventGroupClearBits(system_event_group, EVENT_ERROR_BIT);
        }
    }
}

void watchdog_init(void) {
    // Initializer
}

void watchdog_task_start(void) {
    if (watchdog_task_handle == NULL) {
        xTaskCreate(watchdog_task, "WDTTask", STACK_WATCHDOG_TASK, NULL, PRIO_WATCHDOG_TASK, &watchdog_task_handle);
    }
}
