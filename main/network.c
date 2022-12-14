
#include "network.h"
#include <driver/gpio.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <time.h>
#include "sdkconfig.h"
#include "secrets.h"

EventGroupHandle_t network_event_group;

/** Event handler for WiFi events */
static time_t lastwifireconnect = 0;
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t * ev_co = (wifi_event_sta_connected_t *)event_data;
    wifi_event_sta_disconnected_t * ev_dc = (wifi_event_sta_disconnected_t *)event_data;
    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI("network.c", "WiFi Connected: channel %u bssid %02x%02x%02x%02x%02x%02x",
                           ev_co->channel, ev_co->bssid[0], ev_co->bssid[1], ev_co->bssid[2],
                           ev_co->bssid[3], ev_co->bssid[4], ev_co->bssid[5]);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI("network.c", "WiFi Disconnected: reason %u", ev_dc->reason);
            if (ev_dc->reason == WIFI_REASON_ASSOC_LEAVE) break; /* This was an explicit call to disconnect() */
            if ((lastwifireconnect == 0)
             || ((time(NULL) - lastwifireconnect) > 5)) {
              /* Last reconnect attempt more than 5 seconds ago - try it again */
              ESP_LOGI("network.c", "Attempting WiFi reconnect");
              lastwifireconnect = time(NULL);
              esp_wifi_connect();
            }
            break;
        default: break;
    }
}

/* Event handler for IP_EVENT_(ETH|STA)_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI("network.c", "We got an IP address!");
    ESP_LOGI("network.c", "~~~~~~~~~~~");
    ESP_LOGI("network.c", "IP:     " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI("network.c", "NETMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI("network.c", "GW:     " IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI("network.c", "~~~~~~~~~~~");
    xEventGroupSetBits(network_event_group, NETWORK_CONNECTED_BIT);
}

void network_prepare(void)
{
    /* WiFi does not work without this because... who knows, who cares. */
    nvs_flash_init();
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta(); /* This seems to return a completely useless structure */
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wicfg));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));

    wifi_config_t wccfg = {
      .sta = {
        .ssid = FCO2_WIFISSID,
        .password = FCO2_WIFIPASSWORD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK
      }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wccfg));
}

void network_on(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void network_off(void)
{
    xEventGroupClearBits(network_event_group, NETWORK_CONNECTED_BIT);
    ESP_ERROR_CHECK(esp_wifi_stop());
}

