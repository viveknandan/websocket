#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct SmartSocketDevice
{
    uint64_t group_id;
    uint64_t mac;
}SmartSocketDevice;

// Returns array of devices
uint64_t find_devices_by_group(uint64_t gid,SmartSocketDevice** device_list);
void add_device(uint64_t mac, uint64_t groupid);
void update_device(uint64_t mac, uint64_t groupid);
void remove_device(uint64_t mac);
void remove_all();
uint64_t get_count_of_groups();
uint64_t get_count_of_devices_in_groups(uint64_t gid);
uint64_t convert_mac_to_u64(const uint8_t* mac);
void fill_buffer(uint64_t value,uint8_t* buffer, uint8_t start_index, uint8_t nBytes);
#ifdef __cplusplus
} // EXTERN"C"
#endif