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
#include "nvs_flash.h"
#include "nvs.h"
#include "ssd1306.h"

static TaskHandle_t tx_task_handle;
static TaskHandle_t rx_task_handle;

typedef struct ReadData_
{
    char data[21];
    uint16_t len;
} ReadData;

ReadData rx_data;

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_19)
#define RXD_PIN (GPIO_NUM_20)
#define RELAY_PIN (GPIO_NUM_10)

static QueueHandle_t rxhandle;
static QueueHandle_t txhandle;
static nvs_handle_t app_nvs_handle;
struct uart_data_t
{
    char *data;
    uint8_t len;
};
void init()
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
        return 0;
    }
}

esp_err_t read_nvs_flash(const char *field, int32_t *pData)
{
    int32_t val = 0;
    esp_err_t err = nvs_get_i32(app_nvs_handle, field, &val);
    if (err == ESP_OK)
    {
        *pData = val;
    }
    return err;
}

esp_err_t write_nvs_flash(const char *field, const int32_t *pData)
{
    int32_t val = *pData;
    esp_err_t err = nvs_set_i32(app_nvs_handle, field, val);
    if (err == ESP_OK)
    {
        err = nvs_commit(app_nvs_handle);
    }
    return err;
}

int sendData(const char *logName, const char *data, const int len)
{
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    // ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    struct uart_data_t txdata = {0};
    // esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
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

int send_cmd_wait(char *byte, uint8_t size, const char *msg, uint32_t millisec)
{
    int result = -1;
    ESP_LOGI("SEND:", "Sending command:%s", msg);
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
    static uint8_t current_page = 0;

    char cmd = PAGE(page);
    if (page != current_page)
    {
        send_cmd_wait(&cmd, 1, "Page select cmd", (uint32_t)1000);
    }
}

void start_uart_tasks()
{

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &rx_task_handle);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, &tx_task_handle);
}

void changeBaudRate()
{
    // Serialctrl = 115200* 0.128 = 14745.6 = 14746
    //   = 0x4D or 77 => 600 Baud
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Uartctrl].page);
    uint8_t cmd = CREG_READ(registers[Uartctrl].addr);
    uint32_t reg = send_cmd_wait((char *)&cmd, 1, "Page select cmd", (uint32_t)500);
    reg |= 2458;

    cmd = CREG_WRITE(registers[Uartctrl].addr);
    uint8_t write_cmd[4];
    write_cmd[0] = cmd;
    write_cmd[1] = reg & 0xFF;
    write_cmd[2] = (reg >> 8) & 0xFF;
    write_cmd[3] = (reg >> 16) & 0xFF;
    send_cmd_wait(&write_cmd, 4, "write baud 115200", (uint32_t)500);
    // uart_set_baudrate(UART_NUM_1, 115200);
}

void enableHPFCurrentLine()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Config2].page);
    uint8_t cmd = CREG_READ(registers[Config2].addr);
    uint32_t reg = send_cmd_wait((char *)&cmd, 1, "REad Config2", (uint32_t)500);
    cmd = CREG_WRITE(registers[Config2].addr);
    reg |= 0b10010; //HPF on current channel PMF on voltage channel
    uint8_t write_cmd[4];
    write_cmd[0] = cmd;
    write_cmd[1] = reg & 0xFF;
    write_cmd[2] = (reg >> 8) & 0xFF;
    write_cmd[3] = (reg >> 16) & 0xFF;
    send_cmd_wait(&write_cmd, 4, "write config2", (uint32_t)500);
    // config2 |= 0b01000 //HPF
    // config2 |= 0b10000 //Phase matching
}
void enableHPFVoltageLine()
{
    // config2 |= 0b010 HPF
    // config2  |= 0b100 PM

    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Config2].page);
    uint32_t cmd = CREG_READ(registers[Config2].addr);
    uint32_t reg = send_cmd_wait(&cmd, 1, "REad Config2", (uint32_t)500);
    cmd = CREG_WRITE(registers[Config2].addr);
    reg |= 0b100;
    uint8_t write_cmd[4];
    write_cmd[0] = cmd;
    write_cmd[1] = reg & 0xFF;
    write_cmd[2] = (reg >> 8) & 0xFF;
    write_cmd[3] = (reg >> 16) & 0xFF;
    send_cmd_wait(&write_cmd, 4, "Config2", (uint32_t)500);
}

void cs_write(char page, char addr, uint32_t val)
{
    sendPageSelect(page);
    uint8_t cmd = CREG_WRITE(addr);
    uint8_t write_cmd[4];
    write_cmd[0] = cmd;
    write_cmd[1] = val & 0xFF;
    write_cmd[2] = (val >> 8) & 0xFF;
    write_cmd[3] = (val >> 16) & 0xFF;
    uint32_t reg = send_cmd_wait((char *)&write_cmd, 4, "cs_write", (uint32_t)500);
}

uint32_t cs_read(char page, char addr)
{
    sendPageSelect(page);
    uint8_t cmd = CREG_READ(addr);
    uint32_t reg = send_cmd_wait((char *)&cmd, 1, "cs_read", (uint32_t)500);
    return reg;
}
void setPositiveEnergyOnly()
{
}

void CS5490_hwreset()
{
    gpio_set_level(11, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(11, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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
    value = value;
    ESP_LOGI("CS5490", "Irms = %d => %f", result, value);
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
    ESP_LOGI("CS5490", "Vrms = %d => %f", result, value);
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
    ESP_LOGI("CS5490", "Pf = %d => %f", result, value);
    return value;
}

double readAvgS()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Spower].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Spower].addr);
    int result = send_cmd_wait(&cmd, 1, "Average S read", 500);
    value = CS5490_toDouble(23, MSBsigned, result);
    ESP_LOGI("CS5490", "S = %d => %f", result, value);
    return value;
}

double readAvgQ()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Qavg].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Qavg].addr);
    int result = send_cmd_wait(&cmd, 1, "Average Q read", 500);
    value = CS5490_toDouble(23, MSBsigned, result);
    ESP_LOGI("CS5490", "Q = %d => %f", result, value);
    return value;
}
double readAverageActivePower()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Activeavg].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Activeavg].addr);
    int result = send_cmd_wait(&cmd, 1, "Average Active read", 500);
    value = CS5490_toDouble(23, MSBsigned, result);
    ESP_LOGI("CS5490", "ActivePower = %d => %f", result, value);
    return value;
}

double readFreq()
{
    CS5490Register *registers = getRegisters();
    sendPageSelect(registers[Epsilon].page);
    double value = 1.0;
    char cmd = CREG_READ(registers[Epsilon].addr);
    int result = send_cmd_wait(&cmd, 1, "Freq read", 500);
    value = 4000 * CS5490_toDouble(23, MSBunsigned, result);
    ESP_LOGI("CS5490", "freq = %d => %f", result, value);
    return value;
}

void readAllPower()
{
    CS5490Register *registers = getRegisters();
    // V,I,P,Q,S,F,PF => 21 bytes
    struct uart_data_t txdata;

    sendPageSelect(16);
    uint32_t cmd_resp_buffer[7] = {0};
    char cmd_buffer[7] = {
        CREG_READ(CREG_READ(registers[Rmsvoltage].addr)),
        CREG_READ(registers[Rmscurrent].addr),
        CREG_READ(registers[Activeavg].addr),
        CREG_READ(registers[Qavg].addr),
        CREG_READ(registers[Spower].addr),
        CREG_READ(registers[Epsilon].addr),
        CREG_READ(registers[powerfactor].addr)};
    txdata.data = cmd_buffer;
    txdata.len = 1;
    for (int i = 0; i < 7; i++)
    {
        txdata.data = &cmd_buffer[i];
        xQueueSend(txhandle, &txdata, 0);
        xQueueReceive(rxhandle, &rx_data, 500 / portTICK_PERIOD_MS);
        cmd_resp_buffer[i] = READRESULT(rx_data.data);
    }
    /*
    xQueueSend(txhandle, &txdata, 0);
    xQueueReceive(rxhandle, &rx_data, 5000 / portTICK_PERIOD_MS);
    if (rx_data.len)
    {
        int idx = 0;
        for (int i = 0; i < rx_data.len; i = 1 + 3)
        {
            cmd_resp_buffer[idx] = READRESULT(&rx_data.data[i]);
        }
    }
        */
    double value;
    double vrms;
    vrms = CS5490_toDouble(24, MSBunsigned, cmd_resp_buffer[0]);
    vrms = (((vrms / SYS_GAIN) / V_FS) * V_FS_RMS_V) / V_ALFA;
    ESP_LOGI("CS5490", "Vrms = %d => %f", (int)cmd_resp_buffer[0], vrms);
    double irms;
    irms = CS5490_toDouble(24, MSBunsigned, cmd_resp_buffer[1]);
    irms = (((irms / SYS_GAIN) / I_FS) * I_FS_RMS_V) / R_SHUNT_OHM;
    ESP_LOGI("CS5490", "Irms = %d => %f", (int)cmd_resp_buffer[1], (irms));

    value = CS5490_toDouble(23, MSBsigned, cmd_resp_buffer[2]);
    ESP_LOGI("CS5490", "P = %d => %f", (int)cmd_resp_buffer[2], value);

    value = CS5490_toDouble(23, MSBsigned, cmd_resp_buffer[3]);
    ESP_LOGI("CS5490", "Q = %d => %f", (int)cmd_resp_buffer[3], value);

    value = CS5490_toDouble(23, MSBsigned, cmd_resp_buffer[4]);
    ESP_LOGI("CS5490", "S = %d => %f", (int)cmd_resp_buffer[4], value);

    value = 4000 * CS5490_toDouble(23, MSBunsigned, cmd_resp_buffer[5]);
    ESP_LOGI("CS5490", "freq = %d => %f", (int)cmd_resp_buffer[5], value);

    value = CS5490_toDouble(23, MSBsigned, cmd_resp_buffer[6]);
    ESP_LOGI("CS5490", "pf = %d => %f", (int)cmd_resp_buffer[6], value);
}

void callibrate()
{
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
        ESP_LOGI("CS5490", "Writing Register:%u", (unsigned int)reg);
        send_cmd_wait(write_cmd, 4, registers[Config1].name, 500);
    }
}

void setPGA_CurrrentChanel()
{
    cs_write(0,0,0xC02000|0b100000);
}
void sendSoftReset()
{
    // Send RESET COMMAND
    char cmd = CMD(SOFT_RESET);
    send_cmd_wait(&cmd, 1, "Soft Reset", 500);
}
void CS5490_Test(bool setdefault)
{
    // Verify default content of registers
    CS5490Register *pRegisters = getRegisters();
    int result = 0;
    uint8_t page = 0;

    // if (result > 0)
    // {
    //     ESP_LOGI("CS5490", "Soft reset result: %d", result);
    // }
    // else
    // {
    //     ESP_LOGI("CS5490", "Communication timeout");
    // }

    for (int i = 0; i < getRegisterCount(); i++)
    {
        char write_cmd[4] = {0};
        char cmd;
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
    int result = send_cmd_wait(&cmd, 1, "Starting conversion", 2000);
}

void haltConversion()
{
    char cmd = CMD(HALT);
    int result = send_cmd_wait(&cmd, 1, "Halting conversion", 500);
}

void cs_instruct(char pcmd)
{
    char cmd = CMD(pcmd);
    int result = send_cmd_wait(&cmd, 1, "Send command", 500);
}
void reinstall_uart()
{
    const uart_config_t uart_config = {
        .baud_rate = 19200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // delete tx tx tasks
    vTaskDelete(rx_task_handle);
    vTaskDelete(tx_task_handle);
    // We won't use a buffer for sending data.
    if (uart_is_driver_installed(UART_NUM_1))
    {
        uart_driver_delete(UART_NUM_1);
    }
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // Restore rx tx tasks
    start_uart_tasks();
}

void callibratePowerOffsets()
{
    //Turn off Load
    gpio_set_level(GPIO_NUM_10,0);
    //Reset CS5490
    CS5490_hwreset();
    //
}
void setIDCOffset(double val)
{
    uint32_t val2 = CS5490_toBinary(23,MSBsigned,val);
    cs_write(16,32,val2);
}


void zeroloadcallibration()
{
    //Set Tsettle = 2000
    cs_write(16,57,2000);
    //Set Sample count 16000
    cs_write(16,57,16000);
    //Clear DRDY
    cs_write(0,23,0x800000);
    //Send AC offset callibration 0xF6
    cs_instruct(0xFC);
    //id DRDY set ?
     char DRDY = cs_read(0,23);
     int tries = 10;
     while(DRDY != 0x00 && tries >0)
     {
        DRDY = cs_read(0,23);
        tries--;
     }
  double irms=0;
  uint32_t iaoff=0;
    // Yes then read IRMS, IACOFF
    if(DRDY == 0)
     {
        ESP_LOGI("cs5490","DRDY is zeo. Sucess!!");
        irms  =readIrms();
        iaoff = cs_read(16,37);
     }
    ESP_LOGI("cs5490","IAOFF: %d , irms = %f", (int)iaoff,irms);
    //If IAOFF ==0 then failed
    //else return IACOFF to main flow.
}

void setPoffZero()
{
    //Active P 0.000587
    // Apparent P 0.001791
    cs_write(16,15),;
}
void app_main(void)
{

    int level = 0;
    SSD1306_t ssd1306;
    txhandle = xQueueCreate(10, sizeof(struct uart_data_t));
    rxhandle = xQueueCreate(10, sizeof(ReadData));
    init();
    i2c_master_init(&ssd1306, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&ssd1306, 128, 64);
    ssd1306_display_text(&ssd1306, 1, "Welcome to SmartSocket ", 5, false);
    ssd1306_display_text(&ssd1306, 2, "Initalizating... ", 16, false);
    gpio_set_direction(10, GPIO_MODE_OUTPUT);
    gpio_set_direction(11, GPIO_MODE_OUTPUT);

    start_uart_tasks();

    gpio_set_level(11, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(11, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(10, 1);

    sendSoftReset();
    //enableHPFCurrentLine();
    //setPGA_CurrrentChanel();

    //setIDCOffset(-0.203318);
    //CS5490_Callibrate();
    zeroloadcallibration();
    setDOpinFunction(DO_V_ZERO_CROSSING, 0);
    ssd1306_clear_screen(&ssd1306, false);
    ssd1306_contrast(&ssd1306, 0x0f);
    ssd1306_display_text(&ssd1306, 2, "Ready! ", 6, false);
    ESP_LOGI("CS5490", "Callibration Done");
    const char displayf[] = "%s: %.2f";
    static char display[30] ;
    while (1)
    {
        memset(display,0,30);
        if (level == 0)
        {
            level = 1;
        }
        else
        {
            level = 0;
        }

        // for(i = 0; i< sizeof(cmd_data)/sizeof(Command); i++)
        // {
        //     send_cmd(&cmd_data[i].cmd,cmd_data[i].name);
        //     ESP_LOGI("RESP", "cmd(%s)=>",cmd_data[i].name);
        //     ESP_LOG_BUFFER_HEXDUMP("RESP", rx_data.data, rx_data.len, ESP_LOG_INFO);
        // }
        // haltConversion();

        // readAllPower();
        sendSoftReset();
        startConversion(true);
        //startConversion(true);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        haltConversion();
        double vrms = readVrms();
        double irms = readIrms();
        double activepower = readAverageActivePower();
        double pf = readPf();
        double apparantpower = readAvgS();
        double reactivepower = readAvgQ();
        double freq = readFreq();
        ESP_LOGI("CS5490", "Vrms = %f, Irms = %f, activeP = %f, pf = %f, apparantP = %f, reactiveP = %f freq = %f", vrms, irms, activepower, pf, apparantpower, reactivepower, freq);
        sprintf(display,displayf,"V:",vrms);
        ssd1306_display_text(&ssd1306, 2, display, 30, false);
        memset(display,0,30);
        sprintf(display,displayf,"I:",irms);
        ssd1306_display_text(&ssd1306, 3, display, 30, false);
        ssd1306_contrast(&ssd1306, 0xff);
    }
}
