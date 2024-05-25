#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "diskio_management.h"

static const char *TAG = "FATFS";
static uint8_t disk_mode = FATFS_READONLY_MODE;

// Mount path for the partition
const char *base_path = "/";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

void mount_disk(uint8_t rw_mode)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 8, // 4Kb file size in disk size = 2 Mb
            .format_if_mount_failed = false,
            .allocation_unit_size = 512,
            .disk_status_check_enable = false,
    };
    esp_err_t err = ESP_OK;
    if (rw_mode == FATFS_READONLY_MODE){
        err = esp_vfs_fat_spiflash_mount_ro(base_path, "storage", &mount_config);
        disk_mode = FATFS_READONLY_MODE;
    } else if(rw_mode == FATFS_READWRITE_MODE) {
        err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
        disk_mode = FATFS_READWRITE_MODE;
    }
     
     else
     {
        ESP_LOGE(TAG, "Invalid mode.)");
     }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

}

void unmount_disks()
{

    // Unmount FATFS
    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    if (disk_mode == FATFS_READONLY_MODE){
        ESP_ERROR_CHECK(esp_vfs_fat_spiflash_unmount_ro(base_path, "storage"));
    } else {
        ESP_ERROR_CHECK(esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));
    }
    ESP_LOGI(TAG, "Done");
}
