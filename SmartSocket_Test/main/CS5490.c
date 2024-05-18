#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "CS5490.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>
/* For toDouble and toBinary method */

#ifdef __cplus_plus
extern "C"
{
#endif
extern uint32_t cs_read(char page, char addr);
extern void cs_write(char page, char addr, uint32_t val);
extern void startConversion(bool isCont);
extern void cs_instruct(char cmd);
extern double readIrms();
extern double readVrms();
extern double readPf();
extern double readAvgS();
extern double readAvgQ();
extern double readAverageActivePower();
extern double readFreq();
extern void sendSoftReset();
extern void CS5490_hwreset();
extern void haltConversion();
extern esp_err_t read_nvs_flash(const char *field, int32_t *pData);
extern esp_err_t write_nvs_flash(const char *field, const int32_t *pData);
static CS5490Register cs5490_register[] = {
    {0, 0, 0xc02000, "Config0", 0},
    {1, 0, 0x00eeee, "Config1", 0},
    {3, 0, 0, "Interrupt Mask", 0},
    {5, 0, 0, "Phase compensation", 0},
    {7, 0, 0x02004d, "UART Control", 0},
    {8, 0, 0x01, "PulseWidth", 0},
    {9, 0, 0, "PulseCtrl", 0},
    {23, 0, 0x800000, "Status0", 0},
    {24, 0, 0x801800, "Status1", 0},
    {25, 0, 0, "Status2", 0},
    {34, 0, 0, "RegLock", 0},
    {36, 0, 0, "VPEAK", 0},
    {37, 0, 0, "IPEAK", 0},
    {48, 0, 0, "PSDC", 0},
    {55, 0, 0x000064, "ZX_NUM", 0},
    {0, 16, 0x100200, "Config2", 0},
    {1, 16, 0, "RegChk", 0},
    {2, 16, 0, "I", 0},
    {3, 16, 0, "V", 0},
    {4, 16, 0, "P", 0},
    {5, 16, 0, "PAVG", 0},
    {6, 16, 0, "IRMS", 0},
    {7, 16, 0, "VRMS", 0},
    {14, 16, 0, "QAVG", 0},
    {15, 16, 0, "Q", 0},
    {20, 16, 0, "S", 0},
    {21, 16, 0, "PF", 0},
    {27, 16, 0, "Temp", 0},
    {29, 16, 0, "PSUM", 0},
    {30, 16, 0, "SSUM", 0},
    {31, 16, 0, "QSUM", 0},
    {32, 16, 0, "I_DC_OFF", 0},
    {33, 16, 0x400000, "I_GAIN", 0},
    {34, 16, 0, "V_DC_OFF", 0},
    {35, 16, 0x400000, "VGAIN", 0},
    {36, 16, 0, "P_OFF", 0},
    {37, 16, 0, "I_AC_OFF", 0},
    {49, 16, 0x01999A, "EPSILON", 0}, // Ratio of Line to Sampling frequency
    {51, 16, 0x000FA0, "SampleCount", 0},
    {54, 16, 0x06B716, "T_GAIN", 0},
    {55, 16, 0xD53998, "T_OFF", 0},
    {57, 16, 0x00001E, "TIME_SETTLE", 0},
    {58, 16, 0, "LOAD_MIN", 0},
    {60, 16, 0x500000, "SYS_GAIN", 0},
    {61, 16, 0, "TIME", 0},
    {0, 17, 0, "V_SAG_DUR", 0},
    {1, 17, 0, "V_SAG_LEVEL", 0},
    {4, 17, 0, "I_OVER", 0},
    {5, 17, 0x7FFFFF, "I_OVER_LEVEL", 0},
    {24, 18, 0x100000, "IZX_LEVEL", 0},
    {28, 18, 0x800000, "PulseRate", 0},
    {43, 18, 0x143958, "INT_GAIN", 0},
    {46, 18, 0, "V_SWELL_DUR", 0},
    {47, 18, 0x7FFFFF, "V_SWELL_LEVEL", 0},
    {58, 18, 0x100000, "VZX_LEVEL", 0},
    {62, 18, 0x000064, "CycleCount", 0},
    {63, 18, 0x4CCCCC, "Scale", 0}};

uint8_t getRegisterCount()
{
    return sizeof(cs5490_register) / sizeof(CS5490Register);
}
CS5490Register *getRegisters()
{
    return &cs5490_register;
}

double CS5490_toDouble(int LSBpow, int MSBoption, uint32_t buffer)
{

    double output = 0.0;
    uint8_t MSB = 0;
    uint8_t data[] = {0, 0, 0};
    data[0] = buffer & 0xFF;
    data[1] = (buffer) >> 8 & 0xFF;
    data[2] = (buffer >> 16) & 0xFF;

    switch (MSBoption)
    {

    case MSBnull:
        buffer &= 0x7FFFFF; // Clear MSB
        output = (double)buffer;
        output /= pow(2, LSBpow);
        break;

    case MSBsigned:
        MSB = data[2] & 0x80;
        if (MSB)
        { //- (2 complement conversion)
            buffer = ~buffer;
            buffer = buffer & 0x00FFFFFF; // Clearing the first 8 bits
            output = (double)buffer + 1.0;
            output /= -pow(2, LSBpow);
        }
        else
        { //+
            output = (double)buffer;
            output /= (pow(2, LSBpow) - 1.0);
        }
        break;
    case MSBunsigned:
    {
        output = (double)buffer;
        output /= pow(2, LSBpow);
    }
    break;

    default:
        break;
    }

    return output;
}

/*
  Function: toBinary
  Transforms a double number to a 24 bit number for writing registers

  @param
  LSBpow => Expoent specified from datasheet of the less significant bit
  MSBoption => Information of most significant bit case. It can be only three values:
    MSBnull (1)  The MSB is a Don't Care bit
    MSBsigned (2) the MSB is a negative value, requiring a 2 complement conversion
    MSBunsigned (3) The MSB is a positive value, the default case.
  input => (double) value to be sent to CS5490
 @return binary value equivalent do (double) input

    https://repl.it/@tiagolobao/toDoubletoBinary-CS5490
*/
uint32_t CS5490_toBinary(int LSBpow, int MSBoption, double input)
{

    uint32_t output;

    switch (MSBoption)
    {
    case MSBnull:
        input *= pow(2, LSBpow);
        output = (uint32_t)input;
        output &= 0x7FFFFF; // Clear Don't care bits
        break;
    case MSBsigned:
        if (input <= 0)
        { //- (2 complement conversion)
            input *= -pow(2, LSBpow);
            output = (uint32_t)input;
            output = (output ^ 0xffffffff);
            output = (output + 1) & 0xFFFFFF; // Clearing the first 8 bits
        }
        else
        { //+
            input *= (pow(2, LSBpow) - 1.0);
            output = (uint32_t)input;
        }
        break;
    default:
    case MSBunsigned:
        input *= pow(2, LSBpow);
        output = (uint32_t)input;
        break;
    }

    return output;
}

void CS5490_SendConfiguration()
{
    uint32_t IgainCalibrated;
    uint8_t calibrationData[7];
    uint32_t CriticalRegisterChecksum = 0xDD8BD6;
    ESP_LOGI("Sensor", "Apply default CriticalRegisterChecksum: 0xDD8BD6");
    CriticalRegisterChecksum = 0xDD8BD6;
    // Activate HPF on I current (Don't alterate critical registers checksum)
    uint32_t reg = cs_read(16, 0);

    // Read from eeprom and store in registers
    startConversion(true);
}

void sendCalibrationCommand(uint8_t type, uint8_t channel)
{
    uint8_t calibrationByte = 0b00100000|(Gain | Current);
    calibrationByte |= (Gain | Current);
    calibrationByte = 0b11111001;
    cs_instruct(calibrationByte);
}

uint32_t getRegChk()
{
    return cs_read(16, 1);
}
bool CS5490_testReset(void)
{
    // Activate HPF on I current (to remove the DC component: it's an AC application)
    uint32_t reg = cs_read(16, 0);
    reg |= 0x0000000A;
    cs_write(16, 0, reg);

    // Set to single conversion in order to force CRC computing
    startConversion(false);

    uint32_t regChk = getRegChk();

    if (regChk != 0x00DD8BD6)
    {
        return false;
    }
     ESP_LOGI("CS5490.C","%d", (int)regChk);
    return true; // Reset OK
}

void CS5490_Callibrate()
{
    // The application is AC (mains voltage) with shunt resistor used to measure the current.
    // Perform the Igain calibration with a reference instruments (a calibrated digital power meter) in the condition of more less HALF LOAD
    // Implementation based on "CIRRUS LOGIC AN366REV2" pag. 15 "Main Calibration Flow" https://statics.cirrus.com/pubs/appNote/AN366REV2.pdf
    // Calibration should be performed with PF=1 --> pure resistive load

#define I_CAL_RMS_A 1.3126                                                     // Moreless 1/2 max load
#define SCALE_REGISTER_FRACTION (0.6 * SYS_GAIN * (I_CAL_RMS_A / I_MAX_RMS_A)) // For not full load calibration
#define SCALE_REGISTER_VALUE ((uint32_t)(SCALE_REGISTER_FRACTION * 0x800000))
    bool tuningSubSequenceIsOK = true;
    // Hard reset
    CS5490_hwreset();

    // Set scale register for not full load ac gain callibration
    cs_write(18, 63, SCALE_REGISTER_VALUE);

    startConversion(true);
    haltConversion();

    double rmsV = readVrms();
    double freq = readFreq();
    double rmsI = readIrms();
    double PF = readPf();
    double P = readAverageActivePower();
    double Q = readAvgQ();
    double S = readAvgS();

    // Check values are in expected range else callibration is invalid
    cs_write(16, 57, 0x1F40);
    uint32_t reg = cs_read(16, 57);
    if (reg != 0x1F40)
    {
        tuningSubSequenceIsOK = false;
        ESP_LOGI("CX5490.c","REadback from TSETTLE is not correct.: %x",(unsigned int)reg);
    }
    else
    {
         ESP_LOGI("CX5490.c","Writing to TSETTLESucess");
    }
    reg = cs_read(0, 23);        // Status0 register (in order to manage the DRDY bit)
    cs_write(0, 23, reg&0x7FFFFF); // Clear DRDY by setting it
    uint32_t status0_reg_verify = cs_read(0, 23);
    if ((reg & 0x7FFFFF) != status0_reg_verify)
    {
        ESP_LOGI("CX5490.c","Status0 content: %x",(unsigned int) status0_reg_verify);
        tuningSubSequenceIsOK = false;
    }
    else{
          ESP_LOGI("CX5490.c","DRDY clear Sucess Status0 content: %x",(unsigned int) reg);
    }

    if (tuningSubSequenceIsOK)
    {
        //0b11111001
        sendSoftReset();
        sendCalibrationCommand(Gain, Current);
        bool DRDY = false;
        int timeout = 6;
        while (!DRDY && timeout > 0)
        {
            reg = cs_read(0, 23); // Status0 register (in order to manage the DRDY bit)
            if (reg & 0x800000)
            {
                DRDY = true;
            }
            else
            {
                DRDY = false;
                ESP_LOGI("CX5490.c","Status0 content: %x",(unsigned int) reg);
            }
            timeout--;
        }

        if (DRDY)
        {
            startConversion(true);
            rmsV = readVrms();
            freq = readFreq();
            rmsI = readIrms();
            PF = readPf();
            uint32_t Igain = cs_read(16, 33);
            ESP_LOGI("CS5490.C","vrms: %f, irms: %f, pf: %f, freq: %f", rmsV, rmsI, PF, freq);
            uint32_t IgainReadBack = cs_read(16, 33);
            if (IgainReadBack != Igain)
            {
                ESP_LOGI("CS5490.C","Igain readback not correct. Callibration failed.");
                return;
            }
            startConversion(false); // In order to compute the critical register checksum
            uint32_t regChk = getRegChk();
            ESP_LOGI("CS5490.C","Igain:%x",(unsigned int) Igain);
            ESP_LOGI("CS5490.C","Critical registers checksum:%x ",(unsigned int) regChk); // Must be compared with the one expected for the current configuration (stored in the eeprom). In case of mismatch --> reset CS5490
            ESP_LOGI("CS5490.C","RESET OK! Tuned parameters applied.");

            int32_t calibrationData[6] = {0};
            calibrationData[0] = (uint8_t)(Igain >> 16); // MSB
            calibrationData[1] = (uint8_t)(Igain >> 8);
            calibrationData[2] = (uint8_t)Igain;          // LSB
            calibrationData[3] = (uint8_t)(regChk >> 16); // MSB
            calibrationData[4] = (uint8_t)(regChk >> 8);
            calibrationData[5] = (uint8_t)regChk; // LSB
            uint8_t CRC = 0;
            for (uint8_t i = 0; i < 6; i++)
            {
                CRC ^= calibrationData[i];
            }
            write_nvs_flash("cal0", &(calibrationData[0]));
            write_nvs_flash("cal1", &(calibrationData[1]));
            write_nvs_flash("cal2", &(calibrationData[2]));
            write_nvs_flash("cal3", &(calibrationData[3]));
            write_nvs_flash("cal4", &(calibrationData[4]));
            write_nvs_flash("cal5", &(calibrationData[5]));
            write_nvs_flash("crc", &(CRC));

            // Verify if data was stored
            int32_t read_calibrationData[6];
            read_nvs_flash("cal0", &read_calibrationData[0]);
            read_nvs_flash("cal1", &read_calibrationData[1]);
            read_nvs_flash("cal2", &read_calibrationData[2]);
            read_nvs_flash("cal3", &read_calibrationData[3]);
            read_nvs_flash("cal4", &read_calibrationData[4]);
            read_nvs_flash("cal5", &read_calibrationData[5]);

            for (int i = 0; i < 6; i++)
            {
                if (read_calibrationData[i] != calibrationData[i])
                {
                    ESP_LOGI("CS5490.C","Callibration readback not correct!");
                    break;
                }
            }
        }
        else
        {
            ESP_LOGI("CS5490.C","DREADY failed");
        }
    }
}

#ifdef __cplus_plus
}
#endif