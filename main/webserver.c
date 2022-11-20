
#include <esp_http_server.h>
#include <esp_log.h>
#include <time.h>
#include "webserver.h"

/* These are in foxco2_2022_main.c */
extern float lastco2;
extern float lasttemp;
extern float lasthum;
extern time_t lastvaluets;

static const char startp_p1[] = R"EOSP1(
<html><head><title>FoxCO2-2022</title>
<!-- FIXME Javascript for updates -->
</head><body>
<h1>FoxCO2-2022</h1>
)EOSP1";

static const char startp_p2[] = R"EOSP2(
</body></html>
)EOSP2";

esp_err_t get_startpage_handler(httpd_req_t * req) {
  char myresponse[sizeof(startp_p1) + sizeof(startp_p2) + 1000];
  char * pfp;
  strcpy(myresponse, startp_p1);
  pfp = myresponse + strlen(startp_p1);
  pfp += sprintf(pfp, "<table><tr><th>UpdateTS</th><td id=\"ts\">%ld</td></tr>", lastvaluets);
  pfp += sprintf(pfp, "<tr><th>CO2 (ppm)</th><td id=\"co2\">%.0f</td></tr>", lastco2);
  pfp += sprintf(pfp, "<tr><th>Temperature (C)</th><td id=\"temp\">%.2f</td></tr>", lasttemp);
  pfp += sprintf(pfp, "<tr><th>Humidity (%%)</th><td id=\"hum\">%.1f</td></tr></table>", lasthum);
  strcat(myresponse, startp_p2);
  /* The following two lines are the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_startpage = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_startpage_handler,
    .user_ctx = NULL
};

esp_err_t get_json_handler(httpd_req_t * req) {
  char myresponse[1000];
  char * pfp;
  strcpy(myresponse, "");
  pfp = myresponse;
  pfp += sprintf(pfp, "{\"ts\":%ld,", lastvaluets);
  pfp += sprintf(pfp, "\"co2\":\"%.0f\",", lastco2);
  pfp += sprintf(pfp, "\"temp\":\"%.2f\",", lasttemp);
  pfp += sprintf(pfp, "\"hum\":\"%.1f\"}", lasthum);
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_json = {
    .uri      = "/json",
    .method   = HTTP_GET,
    .handler  = get_json_handler,
    .user_ctx = NULL
};

void webserver_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Documentation is - as usual - a bit patchy, but I assume
     * the following drops the oldest connection if the ESP runs
     * out of connections. */
    config.lru_purge_enable = true;
    config.server_port = 80;

    ESP_LOGI("webserver.c", "Starting webserver on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
      ESP_LOGE("webserver.c", "Failed to start HTTP server.");
      return;
    }
    httpd_register_uri_handler(server, &uri_startpage);
    httpd_register_uri_handler(server, &uri_json);
}

