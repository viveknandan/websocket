| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# Smart Socket Prototype implementation

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is a project to domestarte Smart Socket using WebSocket and ESP WiFi Mesh
## How to use.

### Hardware Required

Smart Socket prototype board running ESP32C6
### Setup the Hardware
Refer to Schematics and HW Manual for more details. Contact author directly on vivekece.ymca@gmail.com

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
