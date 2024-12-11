idf_component_register(SRCS "main.c" "diskio_management.c" "soft_ap.c" "keep_alive.c" "websocket_server.c" "ssd1306.c" "ssd1306_i2c_new.c" "ssd1306_spi.c" "CS5490.c" "web_page.cpp" 
"command_parser.c" "esp_wifi_mesh.c" "device_management.cpp" "smartsocket_nvs_flash.c" "diskio_management.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")
set(image ../webpage)
fatfs_create_rawflash_image(storage ${image} FLASH_IN_PROJECT)