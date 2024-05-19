#include "device_management.h"
#include <forward_list>
#include <cstdlib>
#include <set>
using namespace std;

struct SmartSocketDevice_cpp
{
    SmartSocketDevice dev;
    bool operator==(const SmartSocketDevice_cpp &node)
    {
        return ((dev.group_id == node.dev.group_id && dev.mac == node.dev.mac) ? true : false);
    }
};
forward_list<SmartSocketDevice_cpp> device_list;

uint64_t find_devices_by_group(uint64_t gid, SmartSocketDevice **device_list_in)
{
    uint64_t count = get_count_of_devices_in_groups(gid);
    SmartSocketDevice* llist = NULL;
    if (count > 0)
    {
        llist = (SmartSocketDevice *)malloc(count * sizeof(SmartSocketDevice));
        int index = 0;
        for (auto node : device_list)
        {
            if (node.dev.group_id == gid)
            {
                llist[index].group_id = node.dev.group_id;
                llist[index].mac = node.dev.mac;
                index++;
            }
        }
    }
    *device_list_in = llist;
    return count;
}
void add_device(uint64_t mac, uint64_t groupid)
{
    SmartSocketDevice_cpp dev;
    dev.dev.group_id = groupid;
    dev.dev.mac = mac;
    // First test if device exist
    bool found_dev = false;
    for (auto node : device_list)
    {
        if (node.dev.mac == mac)
        {
            found_dev = true;
            break;
        }
    }
    if (!found_dev)
    {
        device_list.emplace_front(dev);
    }
}
void update_device(uint64_t mac, uint64_t groupid)
{
    for (auto node : device_list)
    {
        if (node.dev.mac == mac)
        {
            node.dev.group_id = groupid;
        }
    }
}
void remove_device(uint64_t mac)
{
    for (auto node : device_list)
    {
        if (node.dev.mac == mac)
        {
            device_list.remove(node);
            break;
        }
    }
}
void remove_all()
{
    device_list.clear();
}

uint64_t get_count_of_groups()
{
    set<uint64_t> group_set;
    for (auto node : device_list)
    {
        group_set.insert(node.dev.group_id);
    }
    return group_set.size();
}

uint64_t get_count_of_devices_in_groups(uint64_t gid)
{
    uint64_t count = 0;
    for (auto node : device_list)
    {
        if (node.dev.group_id == gid)
        {
            count++;
        }
    }
    return count;
}

uint64_t convert_mac_to_u64(const uint8_t *mac)
{
    uint64_t macl = 0;
    if (mac)
    {
        macl |= (uint64_t)(mac[0]);
        macl |= (uint64_t)mac[1] << 8 ;
        macl |= (uint64_t)mac[2] << 16;
        macl |= (uint64_t)mac[3] << 24;
        macl |= (uint64_t)mac[4] << 32;
        macl |= (uint64_t)mac[5] << 40;
    }
    return macl;
}

void fill_buffer(uint64_t value, uint8_t *buffer, uint8_t start_index, uint8_t nBytes)
{
    uint64_t vall = value;
    uint8_t nBytes_ = (nBytes > sizeof(uint64_t)) ? sizeof(uint64_t) : nBytes;
    for (int i = 0; i < nBytes_; i++)
    {
        buffer[i] = vall & 0xFF;
        vall = vall >> 8;
    }
}