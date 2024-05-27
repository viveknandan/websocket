#include "keep_alive.h"
#include "sdkconfig.h"

#include "websocket_server.h"
#include "web_page.h"
extern void wifi_init_softap(void);
#if !CONFIG_HTTPD_WS_SUPPORT
#error This needs HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
    SmartSocketInfo *info;
};

static const char *TAG = "wss_server";
static const size_t max_clients = 4;
extern void SmartSocket_ProcessMessage(const char *);
extern SmartSocketInfo *getSmartSocketInfo();
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    // First receive the full ws message
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len)
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }
    // If it was a PONG, update the keep-alive
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG)
    {
        ESP_LOGD(TAG, "Received PONG message");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                                               httpd_req_to_sockfd(req));

        // If it was a TEXT message, just echo it back
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
        {
            ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
            SmartSocket_ProcessMessage((char *)ws_pkt.payload);
        }
        else
        {
            if (ws_pkt.type == HTTPD_WS_TYPE_PING)
            {
                // Response PONG packet to peer
                ESP_LOGI(TAG, "Got a WS PING frame, Replying PONG");
                ws_pkt.type = HTTPD_WS_TYPE_PONG;
            }
            else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
            {
                // Response CLOSE packet with no payload to peer
                ws_pkt.len = 0;
                ws_pkt.payload = NULL;
            }
            ret = httpd_ws_send_frame(req, &ws_pkt);
        }

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
        ESP_LOGI(TAG, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}

esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "New client connected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return wss_keep_alive_add_client(h, sockfd);
}

void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "Client disconnected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    wss_keep_alive_remove_client(h, sockfd);
    close(sockfd);
}

esp_err_t send_web_page(httpd_req_t *req, uint8_t page_type)
{
    int response = 0;
    char *page = NULL;
    if (page_type == 0)
    {
        page = getLandingPage();
    }

    if (page_type == 1)
    {
        page = getLoginPage();
    }

    if (page_type == 2)
    {
        page = getWelcomePage();
    }
    ESP_LOGI("HTTPSEND", "Web page size : %d", (int)strlen(page));
    response = httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    if (page)
    {
        free(page);
        page = NULL;
    }
    return response;
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *basepath, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(basepath);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, basepath);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req, 0);
}
static esp_err_t login_req_handler(httpd_req_t *req)
{

    return send_web_page(req, 1);
}
static esp_err_t status_req_handler(httpd_req_t *req)
{
    return send_web_page(req, 2);
}

static esp_err_t post_handler(httpd_req_t *req)
{
    return ESP_OK;
}
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[128];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, "/",
                                             req->uri, sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    char *data = getFile(filename);

    /* Send a simple response */
    // ToDo: Authenticate and Manage user sessions
    const char resp[] = "user-session-id";
    httpd_resp_send(req, data, HTTPD_RESP_USE_STRLEN);
    free(data);
    return ESP_OK;
}
static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true};

static const httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
};

static const httpd_uri_t uri_login = {
    .uri = "/login",
    .method = HTTP_GET,
    .handler = login_req_handler,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
};

static const httpd_uri_t uri_welcome = {
    .uri = "/welcome",
    .method = HTTP_GET,
    .handler = status_req_handler,
    .user_ctx = NULL,
    .is_websocket = false,
    .handle_ws_control_frames = false,
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri = "/auth",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};

/* URI handler for getting uploaded files */
httpd_uri_t uri_file_download = {
    .uri = "/*", // Match all URIs of type /path/to/file
    .method = HTTP_GET,
    .handler = download_get_handler,
    .user_ctx = NULL, // Pass server data as context
    .is_websocket = false,
    .handle_ws_control_frames = false,
};

static void send_device_info(void *arg)
{
}
static void send_message(void *arg)
{
    static const char *data = NULL;
    char message_data[] = "{\"objectId\":\"111\",\"device_id:\"123456789\",\"group_id:\"1234\",\"device_name:\"SmartSocket123\",\"relay\":\"1\",\"mac\":\"aa:bb:cc:dd:ee:ff\",\"ip\":\"255.255.255.255\",\"vrms\":\"999.99\",\"irms\":\"999.999\",\"pf\":\"0.0001\",\"freq\":500.11,\"Q\":9999.99,\"S\":9999.99,\"P\":9999.99,\"time\":999999999,\"measure_active\":1}";
    char *msg = NULL;
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    msg = (char *)malloc(3 * sizeof(message_data) / sizeof(char));
    // SmartSocketInfo *info = getSmartSocketInfo();
    SmartSocketInfo *info = resp_arg->info;
    memset(msg, 0, 3 * sizeof(message_data));
    data = getMessage();
    char mac_bytes[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, &mac_bytes);

    int objectid = 2;
    int relay_state = info->relay_state;
    char mac[] = "aa:bb:cc:dd:ee:ff";
    sprintf(mac, "%2x:%2x:%2x:%2x:%2x:%2x", mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);

    char ip[] = "192.168.4.1";
    double vrms = info->vrms;
    double Irms = info->irms;
    double pf = info->pf;
    double freq = info->freq;
    double Q = info->Q;
    double S = info->S;
    double P = info->P;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    unsigned int time_val = tv_now.tv_usec / 1000;
    uint8_t measure_active = info->consversion_started;
    sprintf(msg, data,
            objectid,
            info->id,
            1,
            "SmartSocket",
            info->relay_state,
            mac,
            ip,
            vrms,
            Irms,
            pf,
            freq,
            Q,
            S,
            P,
            (int)time,
            measure_active);
    // ESP_LOGI("WEBSOCKET", "Sending message->%s", msg);
    // ESP_LOGI("WEBSOCKET", "Size = %d, pred: %d", (int)strlen(msg), (int)sizeof(message_data));
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)msg;
    ws_pkt.len = strlen(msg);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    if (resp_arg)
    {
        if (resp_arg->info)
        {
            // free(resp_arg->info);
            resp_arg->info = NULL;
        }
        free(resp_arg);
        resp_arg = NULL;
    }

    if (msg)
    {
        free(msg);
        msg = NULL;
    }
}

static void send_ping(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;
    ws_pkt.type = HTTPD_WS_TYPE_PING;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

bool client_not_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGE(TAG, "Client not alive, closing fd %d", fd);
    httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
    return true;
}

bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGD(TAG, "Checking if client (fd=%d) is alive", fd);
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = wss_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK)
    {
        return true;
    }
    return false;
}

int start_wss_smartplug_server(void)
{
    // Prepare keep-alive engine
    wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
    keep_alive_config.max_clients = max_clients;
    keep_alive_config.client_not_alive_cb = client_not_alive_cb;
    keep_alive_config.check_client_alive_cb = check_client_alive_cb;
    wss_keep_alive_t keep_alive = wss_keep_alive_start(&keep_alive_config);
    // Start the httpd server
    httpd_handle_t server = NULL;
    ESP_LOGI(TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.max_open_sockets = max_clients;
    conf.httpd.global_user_ctx = keep_alive;
    conf.httpd.open_fn = wss_open_fd;
    conf.httpd.close_fn = wss_close_fd;
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[] asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &uri_file_download);
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_login);
    httpd_register_uri_handler(server, &uri_welcome);

    wss_keep_alive_set_user_ctx(keep_alive, server);
    return server;
}

static esp_err_t stop_wss_smartplug_server(httpd_handle_t server)
{
    // Stop keep alive thread
    wss_keep_alive_stop(httpd_get_global_user_ctx(server));
    // Stop the httpd server
    return httpd_ssl_stop(server);
}

void disconnect_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        if (stop_wss_smartplug_server(*server) == ESP_OK)
        {
            *server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop https server");
        }
    }
}

void got_ip_handler(void *arg, esp_event_base_t event_base,
                    int32_t event_id, void *event_data)
{
}

void lost_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
}

void connect_handler(void *arg, esp_event_base_t event_base,
                     int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        *server = start_wss_smartplug_server();
    }
}

// Get all clients and send async message
void wss_server_send_messages(httpd_handle_t *server, SmartSocketInfo *info)
{
    bool send_messages = true;

    if (!*server)
    { // httpd might not have been created by now
        // continue;
        return;
    }
    size_t clients = max_clients;
    int client_fds[max_clients];
    if (httpd_get_client_list(*server, &clients, client_fds) == ESP_OK)
    {
        for (size_t i = 0; i < clients; ++i)
        {
            int sock = client_fds[i];
            if (httpd_ws_get_fd_info(*server, sock) == HTTPD_WS_CLIENT_WEBSOCKET)
            {
                // ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message", sock);
                struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
                resp_arg->hd = *server;
                resp_arg->fd = sock;
                resp_arg->info = info;
                if (httpd_queue_work(resp_arg->hd, send_message, resp_arg) != ESP_OK)
                {
                    ESP_LOGE(TAG, "httpd_queue_work failed!");
                    send_messages = false;
                    break;
                }
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "httpd_get_client_list failed!");
        return;
    }
}
