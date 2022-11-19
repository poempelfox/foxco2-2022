
/* Talking to SCD30 CO2 sensors */

#ifndef _SCD30_H_
#define _SCD30_H_

#include "driver/i2c.h" /* Needed for i2c_port_t */

struct scd30data {
  uint8_t valid;
  uint32_t co2raw;  /* CO2 */
  uint32_t tempraw; /* Temperature */
  uint32_t humraw;  /* Humidity */
  float co2;  /* CO2 */
  float temp; /* Temperature */
  float hum; /* Humidity */
};

/* Initialize the SCD30.
 * Also starts periodic measurements. */
void scd30_init(uint16_t measurementinterval);

/* Start periodic measurements on the SCD30. */
void scd30_startpermeas(uint16_t pressurecorrection);
/* Stop periodic measurements */
void scd30_stoppermeas(void);

/* Read measurement data (particulate matter)
 * from the sensor. */
void scd30_read(struct scd30data * d);

#endif /* _SCD30_H_ */

