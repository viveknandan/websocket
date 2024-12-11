/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "web_page.h"
#include "sys/time.h"
#include "device_management.h"
#include "esp_wifi_mesh.h"
#include "smartsocket_nvs_flash.h"
/*******************************************************
 *                Macros
 *******************************************************/

/*******************************************************
 *                Constants
 *******************************************************/
#define RX_SIZE (1500)
#define TX_SIZE (1460)
#define MAX_PAYLOAD_LEN TX_SIZE - 4

/*******************************************************
 *                Variable Definitions
 *******************************************************/
static const char *MESH_TAG = "mesh_main";
static const uint8_t MESH_ID[6] = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
static uint8_t tx_buf[TX_SIZE] = {
    0,
};
static uint8_t rx_buf[RX_SIZE] = {
    0,
};
static bool is_running = true;
static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static esp_netif_t *netif_sta = NULL;
static QueueHandle_t mesh_child_tx_queue;
static QueueHandle_t mesh_root_tx_queue;

extern void send_smart_socket_info(SmartSocketInfo *info);
extern void execute_command(uint32_t cmd_id);

static void process_cmd_from_root(ChildCmd_t cmd, char *arg, uint32_t arg_len)
{
    ESP_LOGI("MESH", "Processing command: %d", (int)cmd);
    switch (cmd)
    {
    case CHILD_CMD_GET_INFO:
        // Reply to root with node information
        break;
    case CHILD_CMD_START_CNV:
        execute_command(START_MEASURE);
        break;
    case CHILD_CMD_STOP_CNV:
        execute_command(STOP_MEASURE);
        break;
    case CHILD_CMD_RELAY:
        if (arg)
        {
            if (arg[0] == 0)
            {
                execute_command(RELAY_OFF);
            }
            else if (arg[0] == 1)
            {
                execute_command(RELAY_ON);
            }
        }
        break;
    case WEB_PAGE_STATE:
        // ToDo
        break;
    case NEW_USER_ADDED:
        // ToDo
        break;
    case TIME_SYNC:
        // ToDO
        break;
    case USER_REMOVED:
        // ToDo
        break;
    case CHILD_SET_GROUP:
        // ToDo
        break;
    default:
        break;
    }
    if (arg_len > 0 && arg)
    {
        free(arg);
    }
}

static uint64_t get_system_time_in_us()
{
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t time_us = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
    return time_us;
}

static void setTime(uint64_t time)
{
    struct timeval tv_now;
    tv_now.tv_sec = time / 1000000L;
    tv_now.tv_usec = time - tv_now.tv_sec * 1000000L;
    settimeofday(&tv_now, NULL);
}

static void removeUser(char *uname)
{
    // remove user from nvm
}

void send_child_cmd_get_info(void)
{
    // Prepare command
    mesh_smartsocket_ctl_t cmd;
    cmd.cmd = CHILD_CMD_GET_INFO;
    cmd.data_len = 0;
    cmd.group_id = 0;

    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
}

void send_child_cmd_start_cnv(uint64_t devid)
{
    // Prepare command
    uint8_t addr_size = 6;
    mesh_smartsocket_ctl_t cmd;
    cmd.cmd = CHILD_CMD_START_CNV;
    cmd.data_len = 0;
    cmd.group_id = 0;
    // Data payload contains device id which is actually a mac address
    cmd.data = (uint8_t *)malloc(addr_size);
    fill_buffer(devid, cmd.data, 0, addr_size);
    cmd.data_len = addr_size;
    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
}

void send_child_cmd_relay(uint64_t devid, uint8_t state)
{
    // Prepare command
    uint8_t addr_size = 6;
    mesh_smartsocket_ctl_t cmd;
    cmd.cmd = CHILD_CMD_RELAY;
    cmd.data_len = 0;
    cmd.group_id = 0; // broadcast by default
    // Data payload contains device id which is actually a mac address
    cmd.data = (uint8_t *)malloc(addr_size + 1);
    fill_buffer(devid, cmd.data, 0, addr_size);
    cmd.data[addr_size] = state;
    cmd.data_len = addr_size + 1;
    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
}
void send_child_cmd_page_state(uint8_t session_handle, uint8_t page_state, uint32_t session_time_elapsed_in_ms)
{
}
esp_err_t send_child_cmd_new_user_added(uint8_t *user_name, uint8_t *password, bool isadmin)
{
    // Prepare command
    mesh_smartsocket_ctl_t cmd;
    SmartSocketUser user;
    static int32_t uid = 1;
    uint8_t size_data = strlen((char *)user_name);
    uint8_t size_pass = strlen((char *)password);

    if (!(size_data > 0 && size_data <= MAX_UNAME_LEN))
    {
        // Fail
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(user.uname, user_name, size_data);
    memcpy(user.pass, password, size_pass);
    user.is_admin = isadmin;
    user.user_id = uid++;
    cmd.cmd = NEW_USER_ADDED;
    cmd.data_len = size_data + size_pass;
    cmd.group_id = 0;
    // allocate memory
    cmd.data = malloc(sizeof(SmartSocketUser));
    memcpy(cmd.data, &user, sizeof(SmartSocketUser));
    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
    // Save to NVM
    save_user_info(&user);
    return ESP_OK;
}

void send_child_cmd_time_sysnc(uint64_t local_utc_time)
{
    // Prepare command
    mesh_smartsocket_ctl_t cmd;
    cmd.cmd = TIME_SYNC;
    cmd.data_len = sizeof(uint64_t);
    cmd.group_id = 0;
    // allocate memory
    cmd.data = malloc(sizeof(uint64_t));
    // fill data
    fill_buffer(local_utc_time, cmd.data, 0, sizeof(uint64_t));
    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
}
void send_child_cmd_user_removed(char *user)
{
}

void send_child_cmd_set_group(uint64_t devid, uint64_t gid)
{
    // Prepare command
    uint8_t addr_size = 6;
    uint8_t group_size = 8;
    mesh_smartsocket_ctl_t cmd;
    cmd.cmd = CHILD_CMD_START_CNV;
    cmd.data_len = 0;
    cmd.group_id = 0;
    // Data payload contains device id which is actually a mac address
    cmd.data = (uint8_t *)malloc(addr_size + group_size);
    fill_buffer(devid, cmd.data, 0, addr_size);
    fill_buffer(gid, cmd.data, addr_size, group_size);
    cmd.data_len = addr_size + group_size;
    // Push command to Tx queue
    xQueueSend(mesh_root_tx_queue, &cmd, CMD_WAIT_TIME_MS / portTICK_PERIOD_MS);
}

void send_root_selfinfo(SmartSocketInfo *info)
{
    mesh_smartsocket_info_t linfo;
    memcpy(&linfo.info, info, sizeof(SmartSocketInfo));
    ESP_LOGI("CHILD", "Sending message to root node");
    xQueueSend(mesh_child_tx_queue, &linfo, 0);
    if (info)
    {
        free(info);
    }
}

// mesh_light_ctl_t light_on = {
//     .cmd = MESH_CONTROL_CMD,
//     .on = 1,
//     .token_id = MESH_TOKEN_ID,
//     .token_value = MESH_TOKEN_VALUE,
// };

// mesh_light_ctl_t light_off = {
//     .cmd = MESH_CONTROL_CMD,
//     .on = 0,
//     .token_id = MESH_TOKEN_ID,
//     .token_value = MESH_TOKEN_VALUE,
// };

/*******************************************************
 *                Function Declarations
 *******************************************************/

/*******************************************************
 *                Function Definitions
 *******************************************************/
void esp_mesh_p2p_tx_main(void *arg)
{
    // ToDo: If root, get command from user via websocket and send to smartsocket device in the mesh
    // Root node maintains mapping of address and device_id and uses it to lookup device address need to do tx
    esp_err_t err;
    int send_count = 0;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int route_table_size = 0;
    mesh_data_t data;
    data.data = tx_buf;
    data.size = sizeof(tx_buf);
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;

    mesh_smartsocket_ctl_t mesh_root_cmd;
    mesh_smartsocket_info_t smartsocket_info;
    is_running = true;

    while (is_running)
    {
        if (esp_mesh_is_root())
        {
            // Wait for Command to arrive from WebSocket
            xQueueReceive(mesh_root_tx_queue, &mesh_root_cmd, 1000 / portTICK_PERIOD_MS);
            mesh_root_cmd.header.token_id = MESH_TOKEN_ID;
            mesh_root_cmd.header.token_value = MESH_TOKEN_VALUE;
            memcpy(data.data, (uint8_t *)&mesh_root_cmd, sizeof(mesh_smartsocket_ctl_t));
            if (mesh_root_cmd.group_id == 0)
            {
                // Broadcast
                esp_mesh_get_routing_table((mesh_addr_t *)&route_table,
                                           CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);
                for (int i = 0; i < route_table_size; i++)
                {
                    err = esp_mesh_send(&route_table[i], &data, MESH_DATA_P2P, NULL, 0);
                    if (err)
                    {
                        ESP_LOGE(MESH_TAG,
                                 "[ROOT-2-UNICAST:%d][L:%d]parent:" MACSTR " to " MACSTR ", heap:%" PRId32 "[err:0x%x, proto:%d, tos:%d]",
                                 send_count, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                                 MAC2STR(route_table[i].addr), esp_get_minimum_free_heap_size(),
                                 err, data.proto, data.tos);
                    }
                }
            }
        }
        if (!esp_mesh_is_root())
        {
            // Wait for Data to be available and send result to root node
            //  ESP_LOGI(MESH_TAG, "layer:%d, rtableSize:%d, %s", mesh_layer,
            //           esp_mesh_get_routing_table_size(),
            //           (is_mesh_connected && esp_mesh_is_root()) ? "ROOT" : is_mesh_connected ? "NODE"
            //
            ESP_LOGE(MESH_TAG, "Waiting to tx data");
            xQueueReceive(mesh_child_tx_queue, &smartsocket_info, portMAX_DELAY);
            ESP_LOGE(MESH_TAG, "Transmit data to root node.");
            memcpy(data.data, (uint8_t *)&smartsocket_info, sizeof(mesh_smartsocket_info_t));
            err = esp_mesh_send(NULL, &data, MESH_DATA_P2P, NULL, 0);
        }

        // /* if route_table_size is less than 10, add delay to avoid watchdog in this task. */
        // if (route_table_size < 10)
        // {
        //     vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
        // }
    }
    vTaskDelete(NULL);
}

void esp_mesh_p2p_rx_main(void *arg)
{
    int recv_count = 0;
    esp_err_t err;
    mesh_addr_t from;
    int send_count = 0;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_buf;
    data.size = RX_SIZE;
    is_running = true;

    while (is_running)
    {
        data.size = RX_SIZE;
        err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        ESP_LOGE(MESH_TAG, "Received data from mesh err:0x%x, size:%d", err, data.size);
        if (err != ESP_OK || !data.size)
        {
            continue;
        }
        if (esp_mesh_is_root())
        {
            // Receieve smartsocket info from child and send to websockets
            mesh_smartsocket_info_t *info = (mesh_smartsocket_info_t *)data.data;
            SmartSocketInfo *smInfo = malloc(sizeof(SmartSocketInfo));
            memset(smInfo, 0, sizeof(SmartSocketInfo));
            memcpy(smInfo, &info->info, sizeof(SmartSocketInfo));
            //Get src address
            smInfo->id = convert_mac_to_u64(from.addr);
            send_smart_socket_info(smInfo);
        }
        else
        {
            // Receieve command from root node and execute.
            mesh_smartsocket_ctl_t *ctrl_cmd = (mesh_smartsocket_ctl_t *)data.data;
            char *arg = NULL;
            if (ctrl_cmd->data_len > 0)
            {
                arg = malloc(ctrl_cmd->data_len);
            }

            process_cmd_from_root(ctrl_cmd->cmd, arg, ctrl_cmd->data_len);
            // ToDo : Process group Id
        }

        ESP_LOGW(MESH_TAG,
                 "[#RX:%d/%d][L:%d] parent:" MACSTR ", receive from " MACSTR ", size:%d, heap:%" PRId32 ", flag:%d[err:0x%x, proto:%d, tos:%d]",
                 recv_count, send_count, mesh_layer,
                 MAC2STR(mesh_parent_addr.addr), MAC2STR(from.addr),
                 data.size, esp_get_minimum_free_heap_size(), flag, err, data.proto,
                 data.tos);
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_p2p_start(void)
{
    static bool is_comm_p2p_started = false;
    if (!is_comm_p2p_started)
    {
        is_comm_p2p_started = true;
        xTaskCreate(esp_mesh_p2p_tx_main, "MPTX", 3072, NULL, 5, NULL);
        xTaskCreate(esp_mesh_p2p_rx_main, "MPRX", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {
        0,
    };
    static uint16_t last_layer = 0;

    switch (event_id)
    {
    case MESH_EVENT_STARTED:
    {
        esp_mesh_get_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:" MACSTR "", MAC2STR(id.addr));
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED:
    {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, " MACSTR "",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED:
    {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, " MACSTR "",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD:
    {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE:
    {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND:
    {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED:
    {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
        int route_table_size = 0;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:" MACSTR "%s, ID:" MACSTR ", duty:%d",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>"
                                                                   : "",
                 MAC2STR(id.addr), connected->duty);
        last_layer = mesh_layer;
        // mesh_connected_indicator(mesh_layer);
        is_mesh_connected = true;
        if (esp_mesh_is_root())
        {
            esp_netif_dhcpc_stop(netif_sta);
            esp_netif_dhcpc_start(netif_sta);
            // Initalize the devce management
            esp_mesh_get_routing_table((mesh_addr_t *)&route_table,
                                       CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);
            for (int i = 0; i < route_table_size; i++)
            {
                // Only a new device will be added. Group is zero if it is a new device.
                add_device(convert_mac_to_u64(route_table[i].addr), 0);
            }
        }
        esp_mesh_comm_p2p_start();
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED:
    {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        is_mesh_connected = false;
        // mesh_disconnected_indicator();
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_LAYER_CHANGE:
    {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>"
                                                                   : "");
        last_layer = mesh_layer;
        // mesh_connected_indicator(mesh_layer);
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS:
    {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:" MACSTR "",
                 MAC2STR(root_addr->addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED:
    {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:" MACSTR "",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ:
    {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:" MACSTR "",
                 switch_req->reason,
                 MAC2STR(switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK:
    {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:" MACSTR "", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE:
    {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED:
    {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD:
    {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>" MACSTR ", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH:
    {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE:
    {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE:
    {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK:
    {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:" MACSTR "",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH:
    {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, " MACSTR "",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    case MESH_EVENT_PS_PARENT_DUTY:
    {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_PS_PARENT_DUTY>duty:%d", ps_duty->duty);
    }
    break;
    case MESH_EVENT_PS_CHILD_DUTY:
    {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_PS_CHILD_DUTY>cidx:%d, " MACSTR ", duty:%d", ps_duty->child_connected.aid - 1,
                 MAC2STR(ps_duty->child_connected.mac), ps_duty->duty);
    }
    break;
    default:
        ESP_LOGI(MESH_TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}

void start_mdns_service()
{
    // initialize mDNS service
    esp_err_t err = mdns_init();
    if (err)
    {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    // set hostname
    mdns_hostname_set("smartsocket");
    // set default instance
    mdns_instance_name_set("Smart Socket");
}
void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
    start_mdns_service();
    // connect_handler();
}

void start_wifi_mesh()
{
    mesh_root_tx_queue = xQueueCreate(10, sizeof(mesh_smartsocket_ctl_t));
    mesh_child_tx_queue = xQueueCreate(10, sizeof(mesh_smartsocket_info_t));
    /*  create network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    /*  set mesh topology */
    ESP_ERROR_CHECK(esp_mesh_set_topology(CONFIG_MESH_TOPOLOGY));
    /*  set mesh max layer according to the topology */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(128));
#ifdef CONFIG_MESH_ENABLE_PS
    /* Enable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_enable_ps());
    /* better to increase the associate expired time, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(60));
    /* better to increase the announce interval to avoid too much management traffic, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_announce_interval(600, 3300));
#else
    /* Disable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
#endif
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *)&cfg.mesh_id, MESH_ID, 6);
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
    memcpy((uint8_t *)&cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *)&cfg.router.password, CONFIG_MESH_ROUTER_PASSWD,
           strlen(CONFIG_MESH_ROUTER_PASSWD));
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *)&cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
#ifdef CONFIG_MESH_ENABLE_PS
    /* set the device active duty cycle. (default:10, MESH_PS_DEVICE_DUTY_REQUEST) */
    ESP_ERROR_CHECK(esp_mesh_set_active_duty_cycle(CONFIG_MESH_PS_DEV_DUTY, CONFIG_MESH_PS_DEV_DUTY_TYPE));
    /* set the network active duty cycle. (default:10, -1, MESH_PS_NETWORK_DUTY_APPLIED_ENTIRE) */
    ESP_ERROR_CHECK(esp_mesh_set_network_duty_cycle(CONFIG_MESH_PS_NWK_DUTY, CONFIG_MESH_PS_NWK_DUTY_DURATION, CONFIG_MESH_PS_NWK_DUTY_RULE));
#endif
    // ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s<%d>%s, ps:%d", esp_get_minimum_free_heap_size(),
    //          esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed",
    //          esp_mesh_get_topology(), esp_mesh_get_topology() ? "(chain)" : "(tree)", esp_mesh_is_ps_enabled());
}
