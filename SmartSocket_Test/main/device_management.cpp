#include "device_management.h"
#include <forward_list>
#include <cstdlib>
#include <set>
using namespace std;

struct SmartSocketDevice_cpp
{
    SmartSocketDevice dev;
    bool operator==(const SmartSocketDevice_cpp& node)
    {
       return ((dev.group_id == node.dev.group_id && dev.mac == node.dev.mac)?true:false);
    }
};
forward_list<SmartSocketDevice_cpp> device_list;

void find_devices_by_group(uint64_t gid, SmartSocketDevice *device_list_in)
{
    uint64_t count = get_count_of_devices_in_groups(gid);
    device_list_in = (SmartSocketDevice *)malloc(count * sizeof(SmartSocketDevice));
    int index = 0;
    for (auto node : device_list)
    {
        if (node.dev.group_id == gid)
        {
            device_list_in[index].group_id = node.dev.group_id;
            device_list_in[index].mac = node.dev.mac;
        }
    }
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
    set<uint64_t> group_set;
    for (auto node : device_list)
    {
        if (node.dev.group_id == gid)
        {
            group_set.insert(node.dev.group_id);
        }
    }
    return group_set.size();
}

uint64_t convert_mac_to_u64(const uint8_t* mac)
{
   uint64_t macl = 0;
   if(mac)
   {
     macl |= (uint64_t) (mac[0]|mac[1]<<8|mac[2]<<16|mac[3]<<24|mac[4]<<32 | mac[5] <<40);
   }
   return macl;
}

void fill_buffer(uint64_t value,uint8_t* buffer, uint8_t start_index, uint8_t nBytes)
{
    uint64_t vall = value;
    uint8_t nBytes_ = (nBytes > 8)?8:nBytes;
    for(int i = 0; i < nBytes_; i++)
    {
        buffer[i] = vall&0xFF;
        vall = vall>>8;
    }
}