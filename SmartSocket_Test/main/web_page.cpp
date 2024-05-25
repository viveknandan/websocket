#include "web_page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
const char *welcome_page = "/spiflash/welcome.html";
const char *login_page = "/spiflash//login.html";
const char *landing_page = "/spiflash//landing.html";
const char message[] = R"rawliteral(
  {"objectId":"%d",
  "deviceid": "%llu",
  "groupid": "%d",
  "devicename": "%s",
  "relay":"%d",
  "mac":"%s",
  "ip":"%s",
  "vrms":"%.2f",
  "irms":"%.2f",
  "pf": "%.2f",
  "freq": "%.2f",
  "Q": "%.2f",
  "S":"%.2f",
  "P":"%.2f",
  "time":"%d",
  "measure_active":"%d"
})rawliteral";

uint32_t count_bytes_in_file(FILE* fd)
{
  int i = 0;
  char ch;
  do
  {
    /* code */
    ch = getc(fd);
i++;
    
    
  } while (!feof(fd));
  fseek(fd,0,SEEK_SET);
  return i;
}
char *helper(const char *filename)
{
  char *data = NULL;

  const char error_string[] = "Cannot open File";
  uint64_t fsize = sizeof(error_string) + 1;
  struct stat sb;
  if (stat(filename, &sb) == -1)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
    return data;
  }
  FILE *fd = fopen(filename, "r");
  //fsize = (long long)sb.st_size + 1;
  fsize = count_bytes_in_file(fd)+1;
  data = (char *)malloc(fsize);
  memset(data, 0, fsize);
  ESP_LOGI("EBPAGE","Bytes from file : %d bytes",(int)fsize);
  if (fd == NULL)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
    return data;
  }
  //data = fgets(data, fsize - 1, fd);
  int i = 0;
  char ch;

  do
  {
    /* code */
    ch = getc(fd);
  
    if(feof(fd))
    {
      break;
    }
    else
    {
    data[i++] = ch;  
    }
  } while (1);
  
  ESP_LOGI("EBPAGE","Bytes written to buffer : %d bytes",i);
  return data;
}

char *getLandingPage()
{
  return helper(landing_page);
}

char *getWelcomePage()
{

  return helper(welcome_page);
}

char *getLoginPage()
{
  return helper(login_page);
}

const char *getMessage()
{
  return message;
}
