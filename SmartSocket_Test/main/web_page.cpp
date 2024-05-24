#include "web_page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

const char *welcome_page = "/welcome.html";
const char *login_page = "/login.html";
const char *landing_page = "/landing.html";

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
  FILE *fd = fopen(welcome_page, "r");
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

char *getMessage()
{
  return "This is a test message";
}
