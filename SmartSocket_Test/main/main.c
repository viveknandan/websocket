/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "CS5490.h"
#include <math.h>

#include "ssd1306.h"
#include "websocket_server.h"
#include "soft_ap.h"
#include "command_parser.h"
#include "web_page.h"
#include "math.h"
#include "esp_wifi_mesh.h"
#include "diskio_management.h"
#include "smartsocket_nvs_flash.h"

const double theta_error = -0.33;
const double delta_irms_error = 0.35;
static SSD1306_t ssd1306;
;
typedef struct ReadData_
{
    char data[4];
    uint16_t len;
} ReadData;

ReadData rx_data;

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_19)
#define RXD_PIN (GPIO_NUM_20)
#define RELAY_PIN (GPIO_NUM_10)

static QueueHandle_t rxhandle;
static QueueHandle_t txhandle;
static QueueHandle_t cs5490handle;
nvs_handle_t cs_nvshandle;
static httpd_handle_t server = NULL;
static uint64_t select_device_addr = 0;
struct uart_data_t
{
    char *data;
    uint8_t len;
};
void init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // mount disks
    mount_disk(FATFS_READONLY_MODE);
}
// Global functions
void send_smart_socket_info(SmartSocketInfo *info);
void execute_command(uint32_t cmd_id);
int sendData(const char *logName, const char *data, const int len)
{
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    // ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

int send_cmd_wait(char *byte, uint8_t size, const char *msg, uint32_t millisec)
{
    int result = -1;
    // ESP_LOGI("SEND:", "Sending command:%s", msg);
    struct uart_data_t txdata = {0};
    txdata.data = byte;
    txdata.len = size;
    rx_data.len = 0;
    rx_data.data[0] = 0;
    rx_data.data[1] = 0;
    rx_data.data[2] = 0;
    rx_data.data[3] = 0;
    xQueueSend(txhandle, &txdata, 0);
    xQueueReceive(rxhandle, &rx_data, millisec / portTICK_PERIOD_MS);
    if (rx_data.len)
    {
        result = READRESULT(rx_data.data);
    }
    return result;
}

typedef struct Command_
{
    char cmd;
    const char *name;
} Command;

void sendPageSelect(uint8_t page)
{
    // Send Page select command
    static uint8_t lpage;
    int result;
    char cmd = PAGE(page);
    if (lpage != page)
    {
        lpage = page;
        result = send_cmd_wait(&cmd, 1, "Page select cmd", (uint32_t)500);
    }
}

double convertToDouble_unsigned(uint32_t val)
{
    uint32_t product = 0;
    for (int i = 0; i < 24; i++)
    {
        product += pow(2, i) * (val & 0x01);
        val = val >> 1;
    }
    val = (product / pow(2, 24));
    return val;
}

double convertToDouble_signed(uint32_t val)
{
    uint32_t product = 0;
    bool is_negative = (val & (0x01 << 24)) > 0;
    // convert to twos complement
    val = 0xFFFFFF & (~val);
    val = val ^ 0x01;
    double result = convertToDouble_unsigned(val);
    return (is_negative) ? result * -1 : result;
}
double readIrms()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Rmscurrent].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Rmscurrent].addr);
    int result = send_cmd_wait(&cmd, 1, "Irms read", 500);
    // value = convertToDouble_unsigned(result);
    value = CS5490_toDouble(24, MSBunsigned, result);
    value = (((value / SYS_GAIN) / I_FS) * I_FS_RMS_V) / R_SHUNT_OHM;
    // ESP_LOGI("CS5490", "Irms = %d => %f", result, value);
    return value;
}

double readVrms()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Rmsvoltage].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Rmsvoltage].addr);
    int result = send_cmd_wait(&cmd, 1, "Vrms read", 500);
    value = CS5490_toDouble(24, MSBunsigned, result);
    value = (((value / SYS_GAIN) / V_FS) * V_FS_RMS_V) / V_ALFA;
    // ESP_LOGI("CS5490", "Vrms = %d => %f", result, value);
    return value;
}

double readPf()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[powerfactor].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[powerfactor].addr);
    int result = send_cmd_wait(&cmd, 1, "Pf read", 500);
    value = CS5490_toDouble(23, MSBsigned, result);
    value = cos(acosl(value) - acosl(-0.33));

    return value;
}

double readAvgS()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Spower].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Spower].addr);
    int result = send_cmd_wait(&cmd, 1, "Average S read", 500);
    return CS5490_toDouble(23, MSBsigned, result);
}

double readAvgQ()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Qavg].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Qavg].addr);
    int result = send_cmd_wait(&cmd, 1, "Average Q read", 1000);
    return CS5490_toDouble(23, MSBsigned, result);
}
double readAverageActivePower()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Activeavg].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Activeavg].addr);
    int result = send_cmd_wait(&cmd, 1, "Average Q read", 1000);
    value = CS5490_toDouble(23, MSBsigned, result);
    // ESP_LOGI("CS5490", "AvgQ = %d => %f", result, value);
    return value;
}

double readFreq()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Epsilon].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Epsilon].addr);
    int result = send_cmd_wait(&cmd, 1, "Freq read", 1000);
    value = 4000 * CS5490_toDouble(23, MSBunsigned, result);
    // ESP_LOGI("CS5490", "freq = %d => %f", result, value);
    return value;
}

void CS5490_hwreset()
{
    gpio_set_level(11, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(11, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
void setDOpinFunction(DO_Function_t DO_fnct, uint8_t openDrain)
{
    uint32_t reg;
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Config1].page);
    char cmd;
    char write_cmd[4] = {};
    reg = CREG_READ(registers[Config1].addr);
    reg &= (~0x0000000F);

    if (openDrain)
    {
        reg |= 0x00010000;
    }

    if (DO_fnct == DO_EPG)
    {
        reg |= 0x00100000; // Enable EPG block
    }

    if (DO_fnct == DO_EPG || DO_fnct == DO_P_SIGN || DO_fnct == DO_P_SUM_SIGN || DO_fnct == DO_Q_SIGN || DO_fnct == DO_Q_SUM_SIGN || DO_fnct == DO_V_ZERO_CROSSING || DO_fnct == DO_I_ZERO_CROSSING || DO_fnct == DO_HI_Z_DEFAULT)
    {
        reg |= (uint32_t)DO_fnct;
        cmd = CREG_WRITE(registers[Config1].addr);
        write_cmd[0] = cmd;
        write_cmd[1] = reg & 0xFF;
        write_cmd[2] = reg >> 8 & 0xFF;
        write_cmd[3] = reg >> 16 & 0xFF;
        // ESP_LOGI("CS5490", "Writing Register:%u", (unsigned int)reg);
        send_cmd_wait(write_cmd, 4, registers[Config1].name, 1000);
    }
}

void CS5490_Test(bool setdefault)
{
    // Verify default content of registers
    CS5490Register *pRegisters = getRegisters();
    int result = 0;
    uint8_t page = 0;
    // Send RESET COMMAND
    char cmd = CMD(SOFT_RESET);
    char write_cmd[4] = {0};
    result = send_cmd_wait(&cmd, 1, "Soft Reset", 1000);

    if (result > 0)
    {
        ESP_LOGI("CS5490", "Soft reset result: %d", result);
    }
    else
    {
        ESP_LOGI("CS5490", "Communication timeout");
    }

    for (int i = 0; i < getRegisterCount(); i++)
    {

        sendPageSelect(pRegisters[i].page);
        cmd = CREG_READ(pRegisters[i].addr);
        result = send_cmd_wait(&cmd, 1, pRegisters[i].name, (uint32_t)50000);
        if (result >= 0)
        {
            if (result == pRegisters[i].default_value)
            {
                ESP_LOGI("CS5490", "%s command Success.ValRead = %x", pRegisters[i].name, result);
            }
            else
            {
                ESP_LOGW("CS5490", "%s command Success.ValRead = %x  expected = %x", pRegisters[i].name, result, (unsigned int)pRegisters[i].default_value);
                if (setdefault)
                {
                    cmd = CREG_WRITE(pRegisters[i].addr);
                    write_cmd[0] = cmd;
                    write_cmd[1] = pRegisters[i].default_value & 0xFF;
                    write_cmd[2] = pRegisters[i].default_value >> 8 & 0xFF;
                    write_cmd[3] = pRegisters[i].default_value >> 16 & 0xFF;

                    ESP_LOGI("CS5490", "Writing Register:");
                    result = send_cmd_wait(write_cmd, 4, pRegisters[i].name, 1000);
                }
            }
        }
        else
        {
            ESP_LOGE("CS5490", "%s command Failed", pRegisters[i].name);
        }
    }
}

void startConversion(bool isCont)
{
    char cmd = CMD(CONT);
    if (isCont == false)
    {
        cmd = CMD(SINGLE);
    }
    int result = send_cmd_wait(&cmd, 1, "Starting conversion", 3000);
}

bool hasConsversionStarted()
{
    // Read status2 register
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Status2].page);
    char cmd = CREG_READ(registers[Status2].addr);
    int reg = send_cmd_wait(&cmd, 1, "Get conversion status", 1000);
    return ((reg & 11) == 11) ? true : false;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    struct uart_data_t txdata = {0};
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1)
    {
        xQueueReceive(txhandle, &txdata, portMAX_DELAY);
        if (txdata.data)
        {
            // ESP_LOGI(TX_TASK_TAG, "Data valid: Sending, len=%d", (int)txdata.len);
            sendData(TX_TASK_TAG, txdata.data, txdata.len);
        }
        else
        {
            // ESP_LOGI(TX_TASK_TAG, "Data invalid: Not Sending");
        }
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    int i = 0;
    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 250 / portTICK_PERIOD_MS);
        if (rxBytes > 0)
        {
            data[rxBytes] = 0;
            for (i = 0; i < rxBytes; i++)
            {
                // ESP_LOGI(RX_TASK_TAG, "Read %d bytes: %x", rxBytes, data[i]);
                if (i < 4)
                {
                    rx_data.data[i] = data[i];
                    rx_data.len = i + 1;
                }
            }
            // ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            xQueueSend(rxhandle, &rx_data, 0);
        }
    }
    free(data);
}

static void cs5490_read_task()
{
    char cmd;
    uint32_t read_delay = 1000;
    const char cmd_stop_read = 0;
    const char cmd_start_read = 1;
    const char cmd_update_delay = 2;
    bool read_device = false;
    const char displayf[] = "%s: %.2f";
    static char display[30];
    while (1)
    {
        xQueueReceive(cs5490handle, &cmd, 100 / portTICK_PERIOD_MS);
        if (cmd == cmd_stop_read)
        {
            read_device = false;
        }
        if (cmd == cmd_start_read)
        {
            ESP_LOGI("task_cs5490", "Reading values ...");
            read_device = true;
            read_delay = 1000;
            startConversion(true);
            setDOpinFunction(DO_V_ZERO_CROSSING, 0);
            ESP_LOGI("task_cs5490", "Init done");
        }
        if (read_device)
        {
            ESP_LOGI("task_cs5490", "Reading in progress");
            SmartSocketInfo *socketinfo = malloc(sizeof(SmartSocketInfo));
            double vrms = readVrms();
            double irms = roundf((readIrms() - delta_irms_error) * 10);
            irms = irms / 10;
            double pf = readPf();
            if (irms == 0)
            {
                pf = 0; // Undefined
            }
            double apparantpower = vrms * irms;
            double activepower = vrms * irms * pf;
            double reactivepower = vrms * irms * sin(acosl(pf));
            double freq = readFreq();
            ESP_LOGI("CS5490", "Vrms = %f, Irms = %f, activeP = %f, Pf = %f, apparantP = %f, reactiveP = %f, freq = %f", vrms, irms, activepower, pf, apparantpower, reactivepower, freq);
            socketinfo->vrms = vrms;
            socketinfo->irms = irms;
            socketinfo->freq = freq;
            socketinfo->P = apparantpower;
            socketinfo->Q = reactivepower;
            socketinfo->S = activepower;
            socketinfo->pf = pf;
            socketinfo->consversion_started = 1;
            socketinfo->relay_state = gpio_get_level(GPIO_NUM_10);
            send_smart_socket_info(socketinfo);
            memset(display, 0, 30);
            sprintf(display, displayf, "V ", vrms);
            ssd1306_display_text(&ssd1306, 1, display, 30, false);
            memset(display, 0, 30);
            sprintf(display, displayf, "I ", irms);
            ssd1306_display_text(&ssd1306, 2, display, 30, false);
            memset(display, 0, 30);
            sprintf(display, displayf, "Pf ", pf);
            ssd1306_display_text(&ssd1306, 3, display, 30, false);
            ssd1306_contrast(&ssd1306, 0xff);
            // ESP_LOGI("task_cs5490", "Reading done");
        }
        else
        {
        }
        vTaskDelay(read_delay / portTICK_PERIOD_MS);
    }
}
void start_cs5490_task()
{
    cs5490handle = xQueueCreate(10, sizeof(char));
    xTaskCreate(cs5490_read_task, "cs5490_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
}
void start_uart_tasks()
{
    txhandle = xQueueCreate(10, sizeof(struct uart_data_t));
    rxhandle = xQueueCreate(10, sizeof(ReadData));
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 3, NULL);
}

void execute_command(uint32_t cmd_id)
{
    // char mac_bytes[6] = {0};
    // esp_wifi_get_mac(WIFI_IF_STA, &mac_bytes);
    // uint64_t self_device_id = convert_mac_to_u64(mac_bytes);
    // static uint64_t dest_dev_id = 0;
    switch ((Commands)cmd_id)
    {
    case RELAY_ON:
        ESP_LOGI("ProcessMsg", "Setting relay ON!");
        gpio_set_level(GPIO_NUM_10, 1);
        break;
    case RELAY_OFF:
        ESP_LOGI("ProcessMsg", "Setting relay OFF!");
        gpio_set_level(GPIO_NUM_10, 0);
        break;
    case START_MEASURE:
    {
        ESP_LOGI("ProcessMsg", "Start measure!");
        uint8_t task_cmd_start = 1;
        uint8_t task_cmd_update_delay = 3;
        // socketinfo.data_send_interval = (uint32_t)strtol(cmd.arg[0].arg, &ptr, 10);
        // if (socketinfo.data_send_interval >= 1)
        //{
        //  xQueueSend(cs5490handle,&task_cmd_update_delay,0);
        xQueueSend(cs5490handle, &task_cmd_start, 0);
        //}
        // xQueueSend(cs5490handle,&task_cmd_start,0);
        // ESP_LOGI("ProcessMessage", "Interval set to :%d", (int)socketinfo->data_send_interval);
    }
    break;
    case STOP_MEASURE:
    {
        uint8_t task_cmd_stop = 0;
        // if (socketinfo->consversion_started == 1)
        // {
        // Reset CS5490
        xQueueSend(cs5490handle, &task_cmd_stop, 0);
        CS5490_hwreset();
        // socketinfo.consversion_started = 0;
        // }

        ESP_LOGI("ProcessMessage", "Conversion is stopped. CS5490 is reset.");
    }
    break;
        break;
    case SET_TIME:
        break;
    case START_LOG:
        break;
    case STOP_LOG:
        break;
    case DISCOVER:
        // Return list of devices in the mesh network
        // Applicable to root node only
        break;
    case SELECTDEVICE:
        // Broadcast command to device over mesh network
        ESP_LOGI("WEBPAGE","Receieved command Select Device");
        break;
    default:
        break;
    }
}
void SmartSocket_ProcessMessage(const char *msg)
{
    // Parse data
    // Process commands
    // Format:
    /*
    Command structure


cmd:cmd_id,arg1,arg1type,arg2,arg2type,....;
cmd

cmd_id = a positive number
arg_type = 0; integer
arg_type = 1 ; float
arg_type = 1; str



enum Commands
{
  RELAY_ON = 0,
  RELAY_OFF = 1,
  START_MEASURE = 2,
  STOP_MEASURE  =3,
  SET_TIME  =4,
  START_LOG  = 5,
  STOP_LOG = 6,
}


const Commands = Object.freeze({
    RELAY_ON: 0,
    RELAY_OFF: 1,
    START_MEASURE: 2,
    STOP_MEASURE: 3,
    SET_TIME: 4,
    START_LOG: 5,
    STOP_LOG: 6,
});
    */

    Web_command cmd;
    uint32_t cmd_id;
    char *ptr;
    uint32_t anArguments = parse_webcommand(msg, &cmd);
    int len = cmd.arg_len;
    for (int i = 0; i < cmd.arg_len; i++)
    {
        ESP_LOGI("CMD", "%s =>%d:(%s,%s)\n", cmd.id, i, cmd.arg[i].arg, cmd.arg[i].value);
    }
    cmd_id = strtol(cmd.id, &ptr, 10);
    ESP_LOGI("ProcessMsg", "Command recieved:%s -> %d", (char *)cmd.id, (int)cmd_id);
    if(cmd_id == DISCOVER)
    {
        
    }
    if(cmd_id == SELECTDEVICE)
    {
        select_device_addr = (uint64_t)strtol(cmd.arg[0].arg, &ptr, 10);
        ESP_LOGI("WEBPAGE","Select Device: %lu",(unsigned long int)(select_device_addr));
    }
    execute_command(cmd_id);

    free_web_command(cmd);
}

void send_smart_socket_info(SmartSocketInfo *info)
{

    info->data_send_interval = 1;
    memcpy(info->name, "SmartSocket\0", 13);
    if (!esp_mesh_is_root())
    {
        // Child node sends info to root node
        send_root_selfinfo(info);
    }
    else
    {
        // Root node always sends info to websocket
        wss_server_send_messages(&server, info);
    }
}
void app_main(void)
{

    int level = 0;
    init_nvsflash(&cs_nvshandle);
    // ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    init();

    i2c_master_init(&ssd1306, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&ssd1306, 128, 64);
    gpio_set_direction(10, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(11, GPIO_MODE_OUTPUT);

    start_uart_tasks();
    start_cs5490_task();
    CS5490_hwreset();
    gpio_set_level(10, 0);
    ssd1306_clear_screen(&ssd1306, false);
    ssd1306_contrast(&ssd1306, 0x0f);
    ssd1306_display_text(&ssd1306, 1, "Initalized!", 11, false);

    // Start WiFi and Webserver
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_CONNECTED,
                                                        &connect_handler, &server,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_DISCONNECTED,
                                                        &disconnect_handler, &server,
                                                        NULL));
    start_wifi_mesh();
    execute_command(START_MEASURE);
    while (1)
    {
        // //Measurement will automatically start if Relay is switched ON
        // if (gpio_get_level(GPIO_NUM_10))
        // {
        //     execute_command(START_MEASURE);
        // }
        // else
        // {
        //     execute_command(STOP_MEASURE);
        // }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
