#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdbool.h>

// Reusable CLI Framework Command Structure
typedef struct
{
    const char *command;
    const char *help;
    void (*handler)(char *);
} cli_command_t;

// Initialize CLI task and UART settings
void cli_init(void);

// Register a command to the CLI command table
bool cli_register_command(const char *command, const char *help, void (*handler)(char *));

// Start the CLI task
void cli_task_start(void);

// Execute a command line string directly
void cli_execute(char *line);

// Parse input command line
void cli_parse(char *line);

#endif // CLI_H
