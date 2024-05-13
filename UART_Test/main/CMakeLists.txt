idf_component_register(SRCS "soft_ap.c" "keep_alive.c" "websocket_server.c" "ssd1306.c" "ssd1306_i2c_new.c" "ssd1306_spi.c" "CS5490.c" "uart_async_rxtxtasks_main.c" "web_page.cpp" "command_parser.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")
