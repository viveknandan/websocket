#ifndef __CS5490_H
#define __CS5490_H
#endif

#include <stdint.h>
#ifdef __cplus_plus
extern "C"
{
#endif

#define PAGE(addr) (0b10000000 | addr)
#define CREG_READ(addr)  (0b00000000 | addr)
#define CREG_WRITE(addr) (0b01000000 | addr)
#define CMD(code) (0b11000000 | code)

#define SOFT_RESET 0x01
#define STANDBY 0x02
#define WAKEUP 0x03
#define SINGLE 0b00010100
#define CONT 0b00010101
#define HALT 0b00011000
#define READRESULT(data) (0x00FFFFFFLU & (data[0]|data[1]<<8|data[2]<<16))


#define MSBnull 1
#define MSBsigned 2
#define MSBunsigned 3
#define DCoffset 0b00000000
#define ACoffset 0b00010000
#define Gain 0b00011000
// Channel
#define Current 0b00000001
#define Voltage 0b00000010
#define CurrentAndVoltage 0b00000110

/* Default values */
#define MCLK_default 4.096
#define baudRate_default 600

// The following parameters are application dependent, therefore must be properly set according your external hardware
#define R_SHUNT_OHM 0.002           // Application dependent
#define V_ALFA (1000.0 / 1689000.0) // Resistor partition ratio on V input: on CDB5490U 4 * 422kOhm + 1kOhm
#define SYS_GAIN 1.25
#define V_FS 0.6
#define V_FS_RMS_V 0.17676 // 250mVpp
#define V_MAX_RMS (V_FS_RMS_V / V_ALFA)
#define I_FS 0.6
#define I_FS_RMS_V 0.17676 // 250mVpp with GAIN 10x, otherwise with GAIN 50x 50mVpp --> 0.3535 Vrms
#define I_MAX_RMS_A (I_FS_RMS_V / R_SHUNT_OHM)
#define P_FS 0.36
#define P_COEFF ((V_MAX_RMS * I_MAX_RMS_A) / (P_FS * SYS_GAIN * SYS_GAIN))
#define SAMPLE_COUNT_DEFAULT 4000                                              // Number of sample used to compute the output low rate computation (with quartz 4.096MHz --> 1 measure/s).
#define I_CAL_RMS_A 1.3126                                                     // Moreless 1/2 max load
#define SCALE_REGISTER_FRACTION (0.6 * SYS_GAIN * (I_CAL_RMS_A / I_MAX_RMS_A)) // For not full load calibration
#define SCALE_REGISTER_VALUE ((uint32_t)(SCALE_REGISTER_FRACTION * 0x800000))

typedef struct _CS5490Register
{
  const uint8_t addr;
  const uint8_t page; 
  const uint32_t default_value;
  const char* name;
  uint32_t read_value;
}CS5490Register;

typedef enum
{
Config0,
Config1,
Intmask,
Phasecomp,
Uartctrl,
Pulsewidth,
Pulsectrl,
Status0,
Status1,
Status2,
Reglock,
Vpeak,
Ipeak,
Psdc,
Zxnum,
Config2,
Regchk,
Instcurrent,
Instvoltage,
Instantactivepower,
Activeavg,
Rmscurrent,
Rmsvoltage,
Qavg,
Qpower,
Spower,
powerfactor,
Temp,
Psum,
Ssum,
Qsum,
Idcoffset,
Igain,
Vdcoffset,
Vgain,
Poffset,
Iacoffset,
Epsilon,
Samplecount,
Tgain,
Toffset,
Timesettle,
Loadmin,
Sysgain,
Time,
Vsagdur,
Vsaglevel,
Iover,
Ioverlevel,
Izxlevel,
Pulserate,
Intgain,
Vswelldur,
Vswelllevel,
Vzxlevel,
Cyclecount,
Scale
}cs5490_reg_t;

typedef enum AfeGainI_e
{
	I_GAIN_10x = 0x00,
	I_GAIN_50x = 0x01

} AfeGainI_t;

typedef enum DO_Function_e
{
	DO_EPG    = 0,
	DO_P_SIGN = 4,
	DO_P_SUM_SIGN = 6,
	DO_Q_SIGN = 7,
	DO_Q_SUM_SIGN = 9,
	DO_V_ZERO_CROSSING = 11,
	DO_I_ZERO_CROSSING = 12,
	DO_HI_Z_DEFAULT = 14
}
DO_Function_t;



CS5490Register* getRegisters();
uint8_t getRegisterCount();
double CS5490_toDouble(int LSBpow, int MSBoption, uint32_t buffer);

#ifdef __cplus_plus
}
#endif