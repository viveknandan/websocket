#pragma once
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include <esp_https_server.h>

void wss_server_send_messages(httpd_handle_t *server);
void connect_handler(void *arg, esp_event_base_t event_base,
                     int32_t event_id, void *event_data);
void disconnect_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
void got_ip_handler(void *arg, esp_event_base_t event_base,
                    int32_t event_id, void *event_data);
void lost_ip_handler(void *arg, esp_event_base_t event_base,
                     int32_t event_id, void *event_data);