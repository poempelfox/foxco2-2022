
#include <esp_http_server.h>
#include <esp_log.h>
#include <time.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include "webserver.h"
#include "secrets.h"

/* These are in foxco2_2022_main.c */
extern float lastco2;
extern float lasttemp;
extern float lasthum;
extern time_t lastvaluets;

static const char startp_p1[] = R"EOSP1(
<!DOCTYPE html>

<html><head><title>FoxCO2-2022</title>
<style type="text/css">
body { background-color:#000000;color:#cccccc; }
table, th, td { border:1px solid #aaaaff;border-collapse:collapse;padding:5px; }
th { text-align:left; }
td { text-align:right; }
a:link, a:visited, a:hover { color:#ccccff; }
</style>
</head><body>
<h1>FoxCO2-2022</h1>
<noscript>Because you have JavaScript disabled, this cannot
 automatically update, you'll have to reload the page.<br></noscript>
<h2>Currently measured values:</h2>
)EOSP1";

static const char startp_p2[] = R"EOSP2(
<script type="text/javascript">
var getJSON = function(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'json';
    xhr.onload = function() {
      var status = xhr.status;
      if (status === 200) {
        callback(null, xhr.response);
      } else {
        callback(status, xhr.response);
      }
    };
    xhr.send();
};
function updrcvd(err, data) {
  if (err != null) {
    document.getElementById("ts").innerHTML = "---";
    document.getElementById("co2").innerHTML = "----";
    document.getElementById("temp").innerHTML = "--.--";
    document.getElementById("hum").innerHTML = "--.-";
  } else {
    document.getElementById("ts").innerHTML = data.ts;
    document.getElementById("co2").innerHTML = data.co2;
    document.getElementById("temp").innerHTML = data.temp;
    document.getElementById("hum").innerHTML = data.hum;
  }
}
function updatethings() {
  getJSON('/json', updrcvd);
}
var myrefresher = setInterval(updatethings, 30000);
</script>
<br>For querying this data in scripts, you can use
 <a href="/json">the JSON output under /json</a>.
<h2>Firmware-Update:</h2>
Current firmware version:
)EOSP2";

static const char startp_p3[] = R"EOSP3(
<br><form action="/firmwareupdate" method="POST">
Update from URL:
<input type="text" name="updateurl" value="https://www.poempelfox.de/espfw/foxco2-2022.bin">
Admin-Password:
<input type="text" name="updatepw">
<input type="submit" name="su" value="Flash Update">
</form>
Be very patient after clicking "Flash Update", this will take about 30 seconds before anything happens.
</body></html>
)EOSP3";

/********************************************************
 * End of embedded webpages definition                  *
 ********************************************************/

esp_err_t get_startpage_handler(httpd_req_t * req) {
  char myresponse[sizeof(startp_p1) + sizeof(startp_p2) + 1000];
  char * pfp;
  strcpy(myresponse, startp_p1);
  pfp = myresponse + strlen(startp_p1);
  pfp += sprintf(pfp, "<table><tr><th>UpdateTS</th><td id=\"ts\">%ld</td></tr>", lastvaluets);
  if ((lastco2 < 0) || ((time(NULL) - lastvaluets) > 300)) {
    pfp += sprintf(pfp, "<tr><th>CO2 (ppm)</th><td id=\"co2\">----</td></tr>");
    pfp += sprintf(pfp, "<tr><th>Temperature (C)</th><td id=\"temp\">--.--</td></tr>");
    pfp += sprintf(pfp, "<tr><th>Humidity (%%)</th><td id=\"hum\">--.-</td></tr></table>");
  } else {
    pfp += sprintf(pfp, "<tr><th>CO2 (ppm)</th><td id=\"co2\">%.0f</td></tr>", lastco2);
    pfp += sprintf(pfp, "<tr><th>Temperature (C)</th><td id=\"temp\">%.2f</td></tr>", lasttemp);
    pfp += sprintf(pfp, "<tr><th>Humidity (%%)</th><td id=\"hum\">%.1f</td></tr></table>", lasthum);
  }
  const esp_app_desc_t * appd = esp_ota_get_app_description();
  strcat(myresponse, startp_p2);
  pfp = myresponse + strlen(myresponse);
  pfp += sprintf(pfp, "%s version %s compiled %s %s",
                 appd->project_name, appd->version, appd->date, appd->time);
  strcat(myresponse, startp_p3);
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
  if ((lastco2 < 0) || ((time(NULL) - lastvaluets) > 300)) {
    pfp += sprintf(pfp, "\"co2\":\"----\",");
    pfp += sprintf(pfp, "\"temp\":\"--.--\",");
    pfp += sprintf(pfp, "\"hum\":\"--.-\"}");
  } else {
    pfp += sprintf(pfp, "\"co2\":\"%.0f\",", lastco2);
    pfp += sprintf(pfp, "\"temp\":\"%.2f\",", lasttemp);
    pfp += sprintf(pfp, "\"hum\":\"%.1f\"}", lasthum);
  }
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

/* Unescapes a x-www-form-urlencoded string.
 * Modifies the string inplace! */
void unescapeuestring(char * s) {
  char * rp = s;
  char * wp = s;
  while (*rp != 0) {
    if (strncmp(rp, "&amp;", 5) == 0) {
      *wp = '&'; rp += 5; wp += 1;
    } else if (strncmp(rp, "%26", 3) == 0) {
      *wp = '&'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%3A", 3) == 0) {
      *wp = ':'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%2F", 3) == 0) {
      *wp = '/'; rp += 3; wp += 1;
    } else {
      *wp = *rp; wp++; rp++;
    }
  }
  *wp = 0;
}

esp_err_t post_fwup(httpd_req_t * req) {
  char postcontent[600];
  char myresponse[1000];
  char tmp1[600];
  //ESP_LOGI("webserver.c", "POST request with length: %d", req->content_len);
  if (req->content_len >= sizeof(postcontent)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Sorry, your request was too large. Try a shorter update URL?");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  int ret = httpd_req_recv(req, postcontent, req->content_len);
  if (ret < req->content_len) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Your request was incompletely received.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  postcontent[req->content_len] = 0;
  ESP_LOGI("webserver.c", "Received data: '%s'", postcontent);
  if (httpd_query_key_value(postcontent, "updatepw", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No updatepw submitted.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  unescapeuestring(tmp1);
  ESP_LOGI("webserver.c", "UE AdminPW: '%s'", tmp1);
  if (strcmp(tmp1, FCO2_ADMINPW) != 0) {
    httpd_resp_set_status(req, "403 Forbidden");
    strcpy(myresponse, "Admin-Password incorrect.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (httpd_query_key_value(postcontent, "updateurl", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No updateurl submitted.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  unescapeuestring(tmp1);
  ESP_LOGI("webserver.c", "UE UpdateURL: '%s'", tmp1);
  sprintf(myresponse, "OK, will try to update from: %s'", tmp1);
  esp_http_client_config_t httpccfg = {
      .url = tmp1,
      .timeout_ms = 60000,
      .keep_alive_enable = true,
      .crt_bundle_attach = esp_crt_bundle_attach
  };
  ret = esp_https_ota(&httpccfg);
  if (ret == ESP_OK) {
    ESP_LOGI("webserver.c", "OTA Succeed, Rebooting...");
    strcat(myresponse, "OTA Update reported success. Will reboot.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(3 * (1000 / portTICK_PERIOD_MS)); 
    esp_restart();
  } else {
    ESP_LOGE("webserver.c", "Firmware upgrade failed");
    strcat(myresponse, "OTA Update reported failure.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  /* This should not be reached. */
  return ESP_OK;
}

static httpd_uri_t uri_fwup = {
  .uri      = "/firmwareupdate",
  .method   = HTTP_POST,
  .handler  = post_fwup,
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
  /* The default is undocumented, but seems to be only 4k. */
  config.stack_size = 10000;
  ESP_LOGI("webserver.c", "Starting webserver on port %d", config.server_port);
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE("webserver.c", "Failed to start HTTP server.");
    return;
  }
  httpd_register_uri_handler(server, &uri_startpage);
  httpd_register_uri_handler(server, &uri_json);
  httpd_register_uri_handler(server, &uri_fwup);
}

