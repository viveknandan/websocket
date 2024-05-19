#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#define SMART_SOCKET_NAME_LEN 16
const char* getLandingPage();
const char* getWelcomePage();
const char* getLoginPage();
const char* getMessage();

typedef enum 
{
  RELAY_ON = 0,
  RELAY_OFF = 1,
  START_MEASURE = 2,
  STOP_MEASURE  =3,
  SET_TIME  =4,
  START_LOG  = 5,
  STOP_LOG = 6,
  DISCOVER=7,
  SELECTDEVICE =8
}Commands;

typedef enum 
{
    NUMBER,
    DOUBLE,
    STRING

}CommandType;

typedef enum
{
  WEB_STATE_LOGIN,
  WEB_STATE_WELCOME,
  WEB_STATE_LADING,
  WEB_STATE_CONFIG,
  WEB_STATE_TIMEOUT
}WebState;

typedef struct SmartSocketInfo_
{
    uint8_t consversion_started;
    uint64_t id;
    uint8_t name[SMART_SOCKET_NAME_LEN];
    uint32_t data_send_interval;
    uint8_t relay_state;
    double vrms;
    double irms;
    double pf;
    double S;
    double Q;
    double P;
    double freq;
} SmartSocketInfo;
#ifdef __cplusplus
}
#endif