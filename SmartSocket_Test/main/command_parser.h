#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct argument_ 
{
  char* arg;
  char* value;
}Argument;


typedef struct web_command_
{
    char* id;
    Argument arg[5];
    int arg_len;
}Web_command;

uint32_t  parse_webcommand(char* web_message, Web_command* dest);
void free_web_command(Web_command cmd);

#ifdef __cplusplus
} // EXTERN"C"
#endif