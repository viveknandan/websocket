#pragma once
#include <stdint.h>
#include "esp_mesh.h"
#define  MESH_TOKEN_ID       (0x0)
#define  MESH_TOKEN_VALUE    (0xbeef)
#define CMD_WAIT_TIME_MS 500
#ifdef __cplusplus

extern "C"
{
#endif
    typedef enum ChildCmd_t
    {
        CHILD_CMD_GET_INFO,  // Reqest child node to send back mesh_smartsocket_info_t
        CHILD_CMD_START_CNV, // Request child node to start conversion
        CHILD_CMD_STOP_CNV,  // Request child node to stop coversion
        CHILD_CMD_RELAY,     // Request child node to switch on/off the relay => data = duty cycle in percentage ,
        WEB_PAGE_STATE,      // Session_handle,current_state,session_time_elapsed_ms
        NEW_USER_ADDED,      //{username, password,previlege=0 Admin can change anything, 1 = general cannot change configuration, 2 = Read only access}
        TIME_SYNC,           // Current system time; Synced with root node time; Child node should set thier local time
        USER_REMOVED,        //{username}
        CHILD_SET_GROUP      //{group}
    }ChildCmd_t;

    typedef struct mesh_header
    {
        uint8_t token_id;
        uint16_t token_value;
    } mesh_header_t;

    typedef struct mesh_smartsocket_ctl_t
    {
        mesh_header_t header;
        uint8_t cmd;
        uint8_t group_id; // zero group id is used for broadcast
        uint8_t data_len;
        uint8_t* data;
    } mesh_smartsocket_ctl_t;

    typedef struct mesh_smartsocket_info_t
    {
        mesh_header_t header;
        SmartSocketInfo info;
    } mesh_smartsocket_info_t;

    void start_wifi_mesh();
    void send_child_cmd_get_info(void);
    void send_child_cmd_start_cnv(uint64_t devid);
    void send_child_cmd_relay(uint64_t devid, uint8_t state);
    void send_child_cmd_page_state(uint8_t session_handle, uint8_t page_state, uint32_t session_time_elapsed_in_ms);
    esp_err_t send_child_cmd_new_user_added(uint8_t *user_name, uint8_t *password, bool isadmin);
    void send_child_cmd_time_sysnc(uint64_t local_utc_time);
    void send_child_cmd_user_removed(char *user);
    void send_child_cmd_set_group(uint64_t devid,uint64_t gid);
    void send_root_selfinfo(SmartSocketInfo *info);
#ifdef __cplusplus
} // EXTERN"C"
#endif
