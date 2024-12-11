#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum DisMode
    {
        FATFS_READONLY_MODE,
        FATFS_READWRITE_MODE,
        FATFS_INVALID_MODE
    };
    void mount_disk(uint8_t mode);
    void unmount_disk();

#ifdef __cplusplus
} // EXTERN"C"
#endif
