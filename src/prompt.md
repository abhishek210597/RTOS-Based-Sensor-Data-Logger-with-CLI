# Prompt: Design and Develop an RTOS-Based Sensor Data Logger with CLI on ESP32-C6

You are an expert Embedded Systems Engineer specializing in ESP32, FreeRTOS, C programming, RTOS architecture, embedded drivers, UART communication, and low-power IoT systems.

Your task is to design and implement a complete industrial-quality project titled:

# RTOS-Based Sensor Data Logger with Command Line Interface (CLI)

Target Platform:

* ESP32-C6
* Platformio (latest stable version)
* FreeRTOS
* C language only (avoid C++)
* VS Code + Platformio Extension

The project must follow professional embedded software architecture suitable for production-level firmware.

---

# Project Objective

Develop a multitasking sensor data logger capable of:

* Reading data from multiple sensors periodically
* Logging sensor values with timestamps
* Storing logs in memory
* Allowing users to control the system through a UART-based CLI
* Demonstrating RTOS concepts such as multitasking, queues, semaphores, mutexes, timers, and event groups
* Following clean modular architecture

---

# Functional Requirements

Implement the following tasks:

## 1. Sensor Task

# Make it dummy data gen
Priority: Medium

Responsibilities:

* Read sensor values periodically.
* Sampling interval should be configurable.
* Support multiple sensors.

Initially implement:

* Internal temperature (or simulated sensor)
* ADC sensor
* Light sensor (LDR via ADC)
* Optional I2C sensor (BMP280, BME280, SHT31)

Each reading should contain:

```
Timestamp
Sensor ID
Sensor Name
Sensor Value
Status
```

Send readings to Logger Task using FreeRTOS Queue.

---

## 2. Logger Task

Priority: Medium

Responsibilities:

* Receive sensor packets
* Store them inside a circular buffer
* Prevent overflow
* Maintain latest N records

Example:

```
Maximum Logs = 500
```

Each log:

```
typedef struct
{
    uint32_t timestamp;
    uint8_t sensor_id;
    float value;
    uint8_t status;
}sensor_log_t;
```

Logger should also support future storage in:

* SPI Flash
* SD Card
* NVS

Design abstraction layer for storage.

---

## 3. CLI Task

Priority: High

UART-based interactive shell.

Supported commands:

```
help

status

start

stop

clear

logs

log count

sensor list

sensor enable

sensor disable

sensor rate <ms>

memory

tasks

heap

stack

uptime

reboot

version

time

export

stats
```

Example:

```
> help

Available Commands:

help
status
logs
clear
sensor rate
memory
tasks
heap
stack
version
```

Unknown commands should produce:

```
Unknown Command

Type help.
```

CLI must be modular.

Each command should have its own handler.

---

## 4. LED Status Task

Blink LED according to system status.

Examples:

Slow Blink

Idle

Fast Blink

Logging Active

Double Blink

Error

Solid ON

CLI Mode

---

## 5. Watchdog Task

Monitor:

Queue overflow

Task starvation

Stack overflow

Sensor timeout

If detected:

Print warning

Recover if possible

---

## 6. Statistics Task

Every 10 seconds print:

```
CPU Load

Free Heap

Minimum Heap

Task Runtime

Queue Usage

Dropped Logs

Sensor Count

System Uptime
```

---

# RTOS Features to Demonstrate

The project must properly use:

FreeRTOS Tasks

Task Priorities

Queues

Binary Semaphore

Mutex

Recursive Mutex

Software Timers

Event Groups

Task Notifications

Static Allocation where appropriate

Dynamic Allocation only when justified

Critical Sections

Queue Sets (optional)

Idle Hook

Tick Hook (optional)

---

# CLI Features

Implement a reusable CLI framework.

Structure:

```
cli_register_command()

cli_execute()

cli_parse()

cli_help()
```

Each command:

```
typedef struct
{
    char *command;
    char *help;
    void (*handler)(char *);
}cli_command_t;
```

Support:

History (optional)

Command parsing

Argument parsing

Auto help

Error handling

---

# Data Structures

Use:

Circular Buffer

Queue

Linked List (optional)

Ring Buffer

Command Table

Configuration Structure

Task Handle Array

Event Group

---

# Software Architecture

Follow layered architecture.

```
Application

│

├── CLI

├── Logger

├── Sensor

├── Statistics

├── LED

├── Storage

├── Configuration

│

Drivers

│

HAL

│


```

Directory structure:

```
main/

app/

sensor/

logger/

cli/

storage/

drivers/

hal/

config/

include/

components/

docs/
```

---

# Coding Standards

Follow:

MISRA-inspired practices where practical

Modular programming

No global variables unless necessary

Header/source separation

Proper naming conventions

Doxygen comments

Error checking

Return status enums

Use const wherever possible

No magic numbers

Macros for configuration

Thread-safe design

---

# Error Handling

Handle:

Queue Full

NULL Pointer

Invalid CLI Command

Invalid Arguments

Sensor Failure

ADC Failure

UART Failure

Memory Allocation Failure

Task Creation Failure

Semaphore Timeout

Watchdog Timeout

---

# Logging System

Create logging macros.

```
LOG_INFO()

LOG_WARN()

LOG_ERROR()

LOG_DEBUG()
```

Enable compile-time log level selection.

---

# Configuration

Provide configurable options.

```
Sampling Rate

Buffer Size

UART Baudrate

Maximum Sensors

Maximum Logs

Task Stack Size

Task Priority
```

---

# Documentation

Generate:

README

Architecture Diagram

Task Interaction Diagram

Queue Flow Diagram

Memory Layout

CLI Manual

Build Instructions

Flash Instructions

Testing Guide

Future Improvements

---

# Testing

Demonstrate:

Queue Overflow Test

Stress Test

Multiple Sensors

High Sampling Rate

CLI Robustness

Invalid Commands

Memory Leak Check

Long Runtime Test

---

# Deliverables

Provide:

1. Complete Platformio project

2. Folder structure

3. Source code

4. Header files

5. Configuration files

6. Doxygen comments

7. README

8. Architecture explanation

9. FreeRTOS explanation

10. Memory usage explanation

11. CPU utilization explanation

12. Flowcharts

13. UML/Class diagrams where applicable

14. Sequence diagrams

15. Build and flashing instructions

16. Testing procedures

17. Future enhancements

---

# Bonus Features

If possible, additionally implement:

* Wi-Fi-based remote CLI
* MQTT publishing
* SD Card logging
* LittleFS support
* OTA firmware update
* NTP time synchronization
* JSON log export
* CSV export
* Web dashboard
* Sensor calibration
* Power-saving modes
* Secure CLI authentication
* Shell command auto-completion
* Runtime task creation/deletion
* Persistent configuration using NVS

---

# Expected Quality

The firmware should resemble production-quality embedded software with:

* Clean modular architecture
* Reusable components
* Efficient RTOS scheduling
* Thread-safe communication
* Well-documented source code
* Easy scalability for additional sensors
* Professional code organization
* Minimal CPU and RAM usage
* Robust error handling
* High maintainability

Think like a senior embedded firmware engineer working on a commercial IoT product. Explain design decisions before implementation, then implement the project module-by-module with complete, compilable Platformio code.
