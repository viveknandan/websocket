#include "smartsocket_nvs_flash.h"

#include "string.h"
#include "esp_log.h"
enum NvsKey
{
    KEY_NEW_USER_ID_OFF,
    KEY_USER_TABLE_VALID,
    KEY_USER_TABLE_LIST,
    KEY_DEV_NAME,
    KEY_DEV_ID,
    KEY_DEV_GID
};
const char **keys_lookup =
    {
        "uid_off",  // user id offset
        "utab_v",   // Usertable valid flag
        "utab_l",   // usertable list blob
        "dev_name", // device name
        "dev_id",   // SmartSocketDevice id
        "dev_gid"   // SmartSocket group id
};
static nvs_handle_t app_nvs_handle;
SmartSocketUser *user_list[MAX_USER_ENRIES];
char dev_name[MAX_DEV_NAME + 1] = {0};
int8_t read_user_table_valid_flag()
{
    // KEY_USER_TABLE_VALID
    int8_t value = -1;
    ESP_ERROR_CHECK(nvs_set_i8(app_nvs_handle, keys_lookup[KEY_USER_TABLE_VALID], &value));
    return value;
}

void write_user_table_valid_flag(int8_t value)
{
    // KEY_USER_TABLE_VALID
    ESP_ERROR_CHECK(nvs_set_i8(app_nvs_handle, keys_lookup[KEY_USER_TABLE_VALID], value));
    ESP_ERROR_CHECK(nvs_commit(app_nvs_handle));
}

int8_t load_user_table()
{
    // KEY_USER_TABLE_LIST
    uint8_t buffer[sizeof(SmartSocketUser) * MAX_USER_ENRIES + 1];
    SmartSocketUser *user = buffer;
    uint32_t size = sizeof(SmartSocketUser);
    esp_err_t err = nvs_get_blob(app_nvs_handle, keys_lookup[KEY_USER_TABLE_LIST], buffer, size * MAX_USER_ENRIES);
    if (err == ESP_OK)
    {
        for (int i = 0; i < MAX_USER_ENRIES; i++)
        {
            memcpy(user_list[i], user, size);
            user++;
        }
        ESP_LOGI("NVS","User table loaded from NVS");
        return 0;
    }
    ESP_LOGE("NVS","Failed:User table load from NVS");
    return -1;
}

int8_t write_user_table()
{
    // KEY_USER_TABLE_LIST

    uint8_t buffer[sizeof(SmartSocketUser) * MAX_USER_ENRIES + 1];
    SmartSocketUser *user = buffer;
    uint32_t size = sizeof(SmartSocketUser);
    for (int i = 0; i < MAX_USER_ENRIES; i++)
    {
        memcpy(user, user_list[i], size);
        user++;
    }
    esp_err_t err = nvs_set_blob(app_nvs_handle, keys_lookup[KEY_USER_TABLE_LIST], buffer, size * MAX_USER_ENRIES);

    if (err == ESP_OK)
    {
        ESP_ERROR_CHECK(nvs_commit(app_nvs_handle));
        ESP_LOGI("NVS","User table written to NVS");
        return 0;
    }
    ESP_LOGE("NVS","Failed:User table write to NVS");
    return -1;
}

uint32_t new_id_offset()
{
    // KEY_NEW_USER_ID_OFF
    uint32_t val = 0;
    uint32_t nval = 0;
    esp_err_t err = nvs_get_u32(app_nvs_handle, keys_lookup[KEY_NEW_USER_ID_OFF], &val);
    if (err == ESP_OK)
    {
        nval = val + 1;
        nvs_set_u32(app_nvs_handle, keys_lookup[KEY_NEW_USER_ID_OFF], nval);
        ESP_ERROR_CHECK(nvs_commit(app_nvs_handle));
    }
    return val;
}
void set_new_id_offset(uint32_t val)
{
    // KEY_NEW_USER_ID_OFF
    nvs_set_u32(app_nvs_handle, keys_lookup[KEY_NEW_USER_ID_OFF], val);
    ESP_ERROR_CHECK(nvs_commit(app_nvs_handle));
}
static void init_user_list()
{
    // Create user list table in heap
    for (int i = 0; i < MAX_USER_ENRIES; i++)
    {
        SmartSocketUser *user = malloc(sizeof(SmartSocketUser));
        user->user_id = -1;
        memset(user->pass, 0, sizeof(user->pass));
        memset(user->uname, 0, sizeof(user->uname));
        user_list[i] = user;
    }
    // Check if user list entry exist, if not create it
}
int init_nvsflash(nvs_handle_t *nvs_handle_param)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    err = nvs_open("storage", NVS_READWRITE, nvs_handle_param);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return -1;
    }
    else
    {
        printf("Done\n");
        int8_t valid = read_user_table_valid_flag();
        if (valid == 1)
        {
            // Load table from NVS
            load_user_table();
        }
        else
        {
            init_user_list();
            // Create table in NVS with invalid entries
            write_user_table();
            write_user_table_valid_flag(1);
        }
        return 0;
    }
    return -1;
}

void save_device_info_to_nvs(SmartSocketDevice *dev, char *name)
{
    // KEY_DEV_NAME,
    // KEY_DEV_ID,
    // KEY_DEV_GID
    ESP_ERROR_CHECK(nvs_set_u64(app_nvs_handle, keys_lookup[KEY_DEV_ID], dev->mac));
    ESP_ERROR_CHECK(nvs_set_u64(app_nvs_handle, keys_lookup[KEY_DEV_GID], dev->group_id));
    if (name)
    {
        memset(dev_name, 0, MAX_DEV_NAME);
        strncpy(dev_name, name, MAX_DEV_NAME);
        ESP_ERROR_CHECK(nvs_set_str(app_nvs_handle, keys_lookup[KEY_DEV_NAME], dev_name));
    }
}
void retrive_device_info_from_nvs(SmartSocketDevice *dev, char *name)
{
    // KEY_DEV_NAME,
    // KEY_DEV_ID,
    // KEY_DEV_GID
    ESP_ERROR_CHECK(nvs_get_u64(app_nvs_handle, keys_lookup[KEY_DEV_ID], &dev->mac));
    ESP_ERROR_CHECK(nvs_get_u64(app_nvs_handle, keys_lookup[KEY_DEV_GID], &dev->group_id));

    memset(dev_name, 0, MAX_DEV_NAME);
    ESP_ERROR_CHECK(nvs_get_str(app_nvs_handle, keys_lookup[KEY_DEV_NAME], dev_name, MAX_DEV_NAME));
    if (name)
    {
        strncpy(name, dev_name, MAX_DEV_NAME);
    }
}
void save_user_info(const SmartSocketUser *user)
{
    if (user && user->uname[0] && user->pass[0])
    {
        uint8_t len = strlen(user->uname);
        if (len == 0)
        {
            return;
        }
        // check if uname already exist
        bool match = false;
        bool new_entry = false;
        for (int i = 0; i < MAX_USER_ENRIES; i++)
        {
            if (!strncmp(user_list[i]->uname , user->uname,MAX_UNAME_LEN) && (user_list[i]->user_id >= 0))
            {
                // Found, entry already exist, update password
                memset(user_list[i]->pass, 0, MAX_PASS_LEN);
                strncpy(user_list[i]->pass, user->pass, MAX_PASS_LEN);
                match = true;
            }
        }

        if (!match)
        {
            // New entry
            uint32_t id = new_id_offset();
            for (int i = 0; i < MAX_USER_ENRIES; i++)
            {
                if (user_list[i]->user_id == -1)
                {
                    user_list[i]->user_id = id;
                    memset(user_list[i]->uname, 0, MAX_UNAME_LEN);
                    memset(user_list[i]->pass, 0, MAX_PASS_LEN);
                    strncpy(user_list[i]->pass, user->pass, MAX_PASS_LEN);
                    strncpy(user_list[i]->uname, user->uname, MAX_UNAME_LEN);
                    new_entry = true;
                    break;
                }
            }
            if (new_entry)
            {
                write_user_table();
                ESP_LOGI("NVS", "Success: Saved new user");
            }
            else
            {
                // user table is full;
                ESP_LOGE("NVS", "Cannot add user, reached max limit");
            }
        }
    }
}

bool get_user_info(int32_t user_id, SmartSocketUser *user)
{
    bool match = false;
    for (int i = 0; i < MAX_USER_ENRIES; i++)
    {
        if (user_list[i]->user_id == user_id)
        {
            // Found, entry, copy result
            if (user)
            {
                memset(user, 0, sizeof(SmartSocketUser));
                strncpy(user->uname, user_list[i]->uname, MAX_UNAME_LEN);
                strncpy(user->uname, user_list[i]->pass, MAX_PASS_LEN);
                match = true;
            }
        }
    }
    return match;
}

SmartSocketUser **get_all_user()
{
    load_user_table();
    return user_list;
}

void remove_user(int32_t userid)
{
    bool match = false;
    for (int i = 0; i < MAX_USER_ENRIES; i++)
    {
        if (user_list[i]->user_id == userid)
        {
            // Found, entry, copy result
            user_list[i]->user_id = -1;
            match = true;
            break;
        }
    }
    if (match)
    {
        write_user_table();
    }
}