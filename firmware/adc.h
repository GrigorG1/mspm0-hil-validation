/*
 * ======== adc.h ========
 * ADC driver for the potentiometer on PA25 (ADC0 channel 2).
 * Returns raw 12-bit samples (0..4095) referenced to VDDA (~3.3V).
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void     adc_init(void);
uint16_t adc_read(void);   // Blocking; one sample per call.

#endif /* ADC_H */
