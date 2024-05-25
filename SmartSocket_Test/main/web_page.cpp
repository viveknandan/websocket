#include "web_page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

const char *welcome_page = "spiflash/welcome.html";
const char *login_page = "spiflash//login.html";
const char *landing_page = "spiflash//landing.html";
const char message[] = R"rawliteral(
  {"objectId":"%d",
  "deviceid": "%l",
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
  fsize = (long long)sb.st_size + 1;
  data = (char *)malloc(fsize);
  memset(data, 0, fsize);
  if (fd == NULL)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
    return data;
  }
  data = fgets(data, fsize - 1, fd);
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
