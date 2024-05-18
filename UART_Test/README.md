| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# Smart Socket Prototype implementation

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example demonstrates how two asynchronous tasks can use the same UART interface for communication. One can use
this example to develop more complex applications for serial communication.

The example starts two FreeRTOS tasks:
1. The first task periodically transmits `Hello world` via the UART.
2. The second task task listens, receives and prints data from the UART.

## How to use example

### Hardware Required

Smart Socket prototype board running ESP32C6
### Setup the Hardware

The `RXD_PIN` and `TXD_PIN` which are configurable in the code (by default `GPIO4` and `GPIO5`) need to be shorted in
order to receive back the same data which were sent out.

### Configure the project

```
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.
...
```

## Troubleshooting

If you do not see any output from `RX_TASK` then check if you have the `RXD_PIN` and `TXD_PIN` pins shorted on the board.
