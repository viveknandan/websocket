#include "CS5490.h"
#include "math.h"
/* For toDouble and toBinary method */

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
    uint8_t data[] = {0,0,0};
    data[0] = buffer&0xFF;
    data[1] = (buffer) >> 8 & 0xFF;
    data[2] = (buffer >>16) & 0xFF ;

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

