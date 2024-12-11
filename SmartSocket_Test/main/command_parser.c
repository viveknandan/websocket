#include <memory.h>
#include <string.h>

#include "command_parser.h"

#ifdef __cplusplus
extern "C"
{
#endif


//pass dest as null.user must free once processed
uint32_t  parse_webcommand(char* web_message, Web_command* dest)
{
    uint32_t count = 0;
    uint32_t arg_value_counter = 1;
    //First get the count of argument_values
    char* arg_raw = NULL;
    char* token1 = strtok((char*)web_message,":");
    while(token1)
    {
        if(count ==0)
        {
            //printf("Command: %s\n", token1);
            dest->id = (char*)malloc(strlen(token1)+  1);
            memset(dest->id,0,strlen(token1)+  1);
            memcpy(dest->id,token1,strlen(token1));
            
        }
        if(count > 0)
        {
          //printf("Argument: %s",token1);
          arg_raw = calloc(strlen(token1)+1, sizeof(char));
          strncpy(arg_raw,token1,strlen(token1));
          char* token2 = strtok(token1,",");
            while(token2)
            {
                if(arg_value_counter%2 == 1)
                {
                    //printf("arg: %s\n", token2);
                }
                else
                {
                   // printf("arg_value: %s\n", token2);
                }
                arg_value_counter++;
                token2 = strtok(NULL,",");
            }
        }
        
        token1 = strtok(NULL,":");
        count++;
    }
    uint32_t num_of_arguments = (arg_value_counter-1)/2;
    //printf("Raw Arg = %s\n",arg_raw);
       token1 = NULL;
        token1 = strtok(arg_raw,",");
        int cnt = 1;
        int index = 0;
        while(token1)
        {
            if(cnt %2 == 1)
            {
                dest->arg[index].arg = malloc(sizeof(char)*strlen(token1)+1);
                memset(dest->arg[index].arg,0,strlen(token1)+1);
                memcpy(dest->arg[index].arg,token1,strlen(token1));
            }
            else
            {
                dest->arg[index].value =  malloc(sizeof(char)*strlen(token1)+1);
                memset(dest->arg[index].value,0,strlen(token1)+1);
                memcpy(dest->arg[index].value,token1,strlen(token1));
                index++;
            }
            token1 = strtok(NULL,",");
            cnt++;

          }
    
    dest->arg_len = num_of_arguments;
    free(arg_raw);
    arg_raw = NULL;
    return num_of_arguments;
}

void free_web_command(Web_command cmd)
{
    int len = cmd.arg_len;
    for(int i = 0; i < len; i++)
    {
        free(cmd.arg[i].value);
        free(cmd.arg[i].arg);
        cmd.arg[i].value = NULL;
        cmd.arg[i].arg = NULL;
    }
}

#ifdef __cplusplus
} // EXTERN"C"
#endif

