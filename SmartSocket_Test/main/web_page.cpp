#include "web_page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "websocket_server.h"
const char *welcome_page = "/spiflash/welcome.html";
const char *login_page = "/spiflash/login.html";
const char *landing_page = "/spiflash/landing.html";
const char *basepath = "/spiflash";
const char response[] = R"rawliteral({
  "objectId": "%d",
  "resp" : "%d",
  "result" :"%s"
})rawliteral";

const char message_discover[] = R"rawliteral({
  "objectId": "3",
  "deviceid" : "%d",
  "groupid" : "%d",
  "devicename" :"%s"
})rawliteral";

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

#include <stdio.h>
#include <string.h>

const char *getContentType(const char *filename)
{
  const char *extension = strrchr(filename, '.');
  if (extension != NULL)
  {
    if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0)
    {
      return "text/html";
    }
    else if (strcmp(extension, ".css") == 0)
    {
      return "text/css";
    }
    else if (strcmp(extension, ".js") == 0)
    {
      return "application/javascript";
    }
    else if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0)
    {
      return "image/jpeg";
    }
    else if (strcmp(extension, ".png") == 0)
    {
      return "image/png";
    }
    else if (strcmp(extension, ".gif") == 0)
    {
      return "image/gif";
    }
    else if (strcmp(extension, ".pdf") == 0)
    {
      return "application/pdf";
    }
    else
    {
      // Default content type for unknown file types
      return "application/octet-stream";
    }
  }
  // Default content type if no extension is found
  return "application/octet-stream";
}

int main()
{
  const char *filename = "example.html";
  const char *contentType = getContentType(filename);
  printf("Content type for %s: %s\n", filename, contentType);
  return 0;
}

uint32_t count_bytes_in_file(FILE *fd)
{
  int i = 0;
  char ch;
  do
  {
    /* code */
    ch = getc(fd);
    i++;

  } while (!feof(fd));
  fseek(fd, 0, SEEK_SET);
  return i;
}

void send_file_page(httpd_req_t *req, const char *filename)
{
  char *file_path = NULL;
  char *data = NULL;
  uint8_t uri_path_len = 0;
  if (filename)
  {
    uri_path_len = strlen(basepath) + strlen(filename) + 1;
    file_path = (char *)malloc(uri_path_len);
    memset(file_path, 0, uri_path_len);
    strncat(file_path, basepath, uri_path_len);
    strncat(file_path, filename, uri_path_len);
    ESP_LOGI("WEBPAGE", "Requesting file: %s", file_path);
  }

  const uint32_t chunk_size = 128;
  const char error_string[] = "Cannot open File";
  uint64_t fsize = sizeof(error_string) + 1;
  struct stat sb;
  if (stat(file_path, &sb) == -1)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
  }
  FILE *fd = fopen(file_path, "rb");

  if (fd == NULL)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
    httpd_resp_send(req, data, fsize);
    free(data);
    data = NULL;
    return;
  }

  if (data)
  {
    httpd_resp_send(req, data, fsize);
    free(data);
    data = NULL;
    return;
  }
  //Set response content type
  httpd_resp_set_type(req, getContentType(file_path));
  ESP_LOGI("EBPAGE","Content type is: %s",getContentType(file_path));
  if (file_path)
  {
    free(file_path);
    file_path = NULL;
  }
  fsize = (long long)sb.st_size + 1;
  // fsize = count_bytes_in_file(fd) + 1;
  ESP_LOGI("EBPAGE", "Bytes from file : %d bytes", (int)fsize);
  // data = fgets(data, fsize - 1,  fd);
  uint32_t size_read = 0;
  uint32_t bytes_read = 0;
  char data_chunk[chunk_size] = {0};

  do
  {
    /* code */
    memset(data_chunk, 0, chunk_size);
    size_read = fread(data_chunk, sizeof(char), chunk_size, fd);
    bytes_read += size_read;
    if (req)
    {
      httpd_resp_send_chunk(req, data_chunk, size_read);
    }
    else
    {
      ESP_LOGE("WEBPAGE", "Req is NULL");
    }
  } while (size_read == chunk_size);
  //} while (size_read == chunk_size && bytes_read < fsize);
  fclose(fd);
  fd =  NULL;
  httpd_resp_send_chunk(req, data_chunk, 0);
  ESP_LOGI("EBPAGE", "Bytes written to buffer : %u bytes, file size was : %u bytes", (unsigned int)bytes_read, (unsigned int)fsize);
}

char *helper(const char *filename)
{
  char *data = NULL;
  const uint32_t chunk_size = 1024;
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

  if (fd == NULL)
  {
    data = (char *)malloc(fsize);
    strncpy(data, error_string, fsize);
    return data;
  }
  fsize = (long long)sb.st_size + 1;
  // fsize = count_bytes_in_file(fd) + 1;
  data = (char *)malloc(fsize);
  if (data)
  {
    memset(data, 0, fsize);
  }
  else
  {
    ESP_LOGW("WEBPAGE", "Cannot malloc : out of memory");
    return NULL;
  }

  ESP_LOGI("EBPAGE", "Bytes from file : %d bytes", (int)fsize);
  // data = fgets(data, fsize - 1,  fd);
  uint32_t size_read = 0;
  char data_chunk[chunk_size] = {0};
  char *ch = data;
  uint32_t bytes_read = 0;
  do
  {
    /* code */
    size_read = fread(data_chunk, sizeof(char), chunk_size, fd);
    if (ch)
    {
      memcpy(ch, data_chunk, size_read);
      ch += size_read;
      bytes_read += size_read;
    }

  } while (size_read == chunk_size && bytes_read < fsize);

  ESP_LOGI("EBPAGE", "Bytes written to buffer : %u bytes, file size was : %u bytes", (unsigned int)bytes_read, (unsigned int)fsize);
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

char *getFile(char *filename)
{
  char *file_path = NULL;
  char *data = NULL;
  uint8_t uri_path_len = 0;
  if (filename)
  {
    uri_path_len = strlen(basepath) + strlen(filename) + 1;
    file_path = (char *)malloc(uri_path_len);
    memset(file_path, 0, uri_path_len);
    strncat(file_path, basepath, uri_path_len);
    strncat(file_path, filename, uri_path_len);
    ESP_LOGI("WEBPAGE", "Requesting file: %s", file_path);
  }
  data = helper(file_path);
  if (file_path)
  {
    free(file_path);
    file_path = NULL;
  }

  return (data);
}
