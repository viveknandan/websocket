#pragma once
#include "device_management.h"
#include "stdbool.h"
#include "nvs_flash.h"
#include "nvs.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MAX_USER_ENRIES 5
#define MAX_UNAME_LEN 15
#define MAX_PASS_LEN 15
#define MAX_DEV_NAME 15

typedef struct SmartSockerUser_{
    int32_t user_id;
    char uname[MAX_UNAME_LEN+1];
    char pass[MAX_PASS_LEN+1];
    bool is_admin;
}SmartSocketUser;

int init_nvsflash(nvs_handle_t *nvs_handle_param);
void save_device_info_to_nvs(SmartSocketDevice* dev,char* dev_name);
void retrive_device_info_from_nvs(SmartSocketDevice* dev,char*dev_name);
void save_user_info(const SmartSocketUser* user);
bool get_user_info(int32_t user_id, SmartSocketUser *user);
SmartSocketUser ** get_all_user();
void remove_user(int32_t userid);
#ifdef __cplusplus
} // EXTERN"C"
#endif