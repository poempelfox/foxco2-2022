/* Talking to SCD30 CO2 sensors */

#include "esp_log.h"
#include "scd30.h"
#include "sdkconfig.h"
#include <string.h>


#define SCD30ADDR 0x61

#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication (in millisec) */

static i2c_port_t scd30i2cport = 0;

void scd30_init(uint16_t measinterval)
{
    esp_err_t ret;
    /* Configure measurement interval */
    uint8_t cmdmi[4] = { 0x46, 0x00, 0x00, 0x00 };
    cmdmi[2] = ((measinterval >> 8) & 0xff);
    cmdmi[3] = ((measinterval >> 0) & 0xff);
    ret = i2c_master_write_to_device(scd30i2cport, SCD30ADDR,
                                     cmdmi, sizeof(cmdmi),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
      ESP_LOGW("scd30.c", "Sensor init command 1 failed: code %d %s", ret, esp_err_to_name(ret));
    }
    /* Activate Automatic Self Calibration. It shouldn't hurt
     * even if the conditions required for it to work (1 hour
     * of fresh air per day) are not met, it should just do
     * nothing. */
    uint8_t cmdasc[4] = { 0x53, 0x06, 0x00, 0x01 };
    ret = i2c_master_write_to_device(scd30i2cport, SCD30ADDR,
                                     cmdasc, sizeof(cmdasc),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
      ESP_LOGW("scd30.c", "Sensor init command 2 failed: code %d %s", ret, esp_err_to_name(ret));
    }
    /* Start periodic measurements without pressure correction */
    scd30_startpermeas(0);
}

void scd30_startpermeas(uint16_t presscorr)
{
    uint8_t cmd[4] = { 0x00, 0x10, 0x00, 0x00 };
    cmd[2] = ((presscorr >> 8) & 0xff);
    cmd[3] = ((presscorr >> 0) & 0xff);
    esp_err_t ret;
    ret = i2c_master_write_to_device(scd30i2cport, SCD30ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
      ESP_LOGW("scd30.c", "Start measurement command failed: code %d %s", ret, esp_err_to_name(ret));
    }
}

void scd30_stoppermeas(void)
{
    uint8_t cmd[2] = { 0x01, 0x04 };
    i2c_master_write_to_device(scd30i2cport, SCD30ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /* FIXME? we ignore the return value and just assume success. */
}

/* This function is based on Sensirons example code and datasheet
 * for the SHT3x and was written for that. CRC-calculation is
 * exactly the same for the SCD30, so we reuse it. */
static uint8_t scd30_crc(uint8_t b1, uint8_t b2)
{
    uint8_t crc = 0xff; /* Start value */
    uint8_t b;
    crc ^= b1;
    for (b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x131;
      } else {
        crc = crc << 1;
      }
    }
    crc ^= b2;
    for (b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x131;
      } else {
        crc = crc << 1;
      }
    }
    return crc;
}

void scd30_read(struct scd30data * d)
{
    uint8_t readbuf[18];
    uint8_t cmd[2] = { 0x03, 0x00 };
    i2c_master_write_to_device(scd30i2cport, SCD30ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    d->valid = 0;
    d->co2raw = 0xffffffff;  d->tempraw = 0xffffffff; d->humraw = 0xffffffff;
    d->co2 = -999.99; d->temp = -999.9; d->hum = -999.99;
    /* Datasheet says we need to give the sensor at least 20 ms time before
     * we can read the data so that it can fill its internal buffers */
    vTaskDelay(pdMS_TO_TICKS(22));
    int res = i2c_master_read_from_device(scd30i2cport, SCD30ADDR,
                                          readbuf, sizeof(readbuf),
                                          I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGI("scd30.c", "ERROR: I2C-read from SCD30 failed.");
      return;
    }
    /* Check CRC */
    for (int p = 0; p < 6; p++) {
      if (scd30_crc(readbuf[(p * 3) + 0], readbuf[(p * 3) + 1]) != readbuf[(p * 3) + 2]) {
        ESP_LOGI("scd30.c", "ERROR: CRC-check for read part %d failed.", p + 1);
        return;
      }
    }
    /* OK, CRC matches, this is looking good. */
    d->co2raw = ((uint32_t)readbuf[0] << 24) | ((uint32_t)readbuf[1] << 16)
              | ((uint32_t)readbuf[3] <<  8) | ((uint32_t)readbuf[4] <<  0);
    d->tempraw = ((uint32_t)readbuf[6] << 24) | ((uint32_t)readbuf[7]  << 16)
               | ((uint32_t)readbuf[9] <<  8) | ((uint32_t)readbuf[10] <<  0);
    d->humraw = ((uint32_t)readbuf[12] << 24) | ((uint32_t)readbuf[13] << 16)
              | ((uint32_t)readbuf[15] <<  8) | ((uint32_t)readbuf[16] <<  0);
    memcpy(&(d->co2), &(d->co2raw), 4);
    memcpy(&(d->temp), &(d->tempraw), 4);
    memcpy(&(d->hum), &(d->humraw), 4);
    /* Mark the result as valid. */
    d->valid = 1;
}

