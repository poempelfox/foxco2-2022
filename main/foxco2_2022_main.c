/* Foxis CO2 sensor firmware, 2022 edition.
 */
#include "sdkconfig.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <soc/i2c_reg.h>
#include <time.h>
#include <esp_sntp.h>
#include "network.h"
#include "scd30.h"
#include "webserver.h"

float lastco2 = -999;
float lasttemp = -999.99;
float lasthum = -999.9;
time_t lastvaluets = 0;

void i2cport_init(void)
{
    i2c_config_t i2cp0conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 33,  /* GPIO33 */
        .scl_io_num = 25,  /* GPIO25 */
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000, /* There is really no need to hurry */
    };
    i2c_param_config(0, &i2cp0conf);
    if (i2c_driver_install(0, i2cp0conf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGI("main.c", "Oh dear: I2C-Init for Port 0 failed.");
    } else {
        ESP_LOGI("main.c", "I2C master port 0 initialized");
    }
    /* I really hate Espressifs patchy documentation.
     * The following is required to permit the slave to do clock stretching.
     * We want to set this to the maximum, but the maximum seems to be
     * dependant on the chip, and it isn't documented what those maximums
     * are or how to get them. There might be an (undocumented) define
     * I2C_TIME_OUT_REG_V, but not sure it's really there.
     * If the following lines throw a compile error, replace the
     * I2C_TIME_OUT_REG_V with 0xfffff and if that throws a warning
     * during runtime, decrease it from there until it no longer does. */
    ESP_LOGI("main.c", "Trying to set timeout to %x", I2C_TIME_OUT_REG_V);
    i2c_set_timeout(0, I2C_TIME_OUT_REG_V);
}

void app_main(void)
{
    i2cport_init();
    scd30_init(55);
    network_prepare();
    network_on(); /* We just stay connected */
    /* Wait for up to 5 seconds to connect to WiFi and get an IP */
    xEventGroupWaitBits(network_event_group, NETWORK_CONNECTED_BIT,
                        pdFALSE, pdFALSE,
                        (5000 / portTICK_PERIOD_MS));
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp2.fau.de");
    sntp_setservername(1, "ntp3.fau.de");
    sntp_init();
    webserver_start();
    time_t lastread = time(NULL);
    while (1) {
      time_t curts = time(NULL);
      if (((curts - lastread) >= 60) || (lastread > curts)) {
        lastread = curts;
        ESP_LOGI("main.c", "Reading CO2 sensor...");
        struct scd30data d;
        scd30_read(&d);
        ESP_LOGI("main.c", "Read values: valid = %d; CO2 raw 0x%x = %.0f ppm; temp raw 0x%x = %.2f deg; hum raw = 0x%x = %.2f%%",
                           d.valid, d.co2raw, d.co2, d.tempraw, d.temp, d.humraw, d.hum);
        fflush(stdout);
        if (d.valid) { /* Update our global variables, so the webserver can export them */
          lastco2 = d.co2;
          lasttemp = d.temp;
          lasthum = d.hum;
          lastvaluets = time(NULL);
        }
      }
      vTaskDelay(20 * (1000 / portTICK_PERIOD_MS));
    }
}

