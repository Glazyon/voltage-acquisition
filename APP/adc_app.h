#ifndef __ADC_APP_H_
#define __ADC_APP_H_

#include "stdint.h"

void adc_task(void);
void adc_dma_init(void);
void adc_tim_dma_init(void);

// Hide mode conversion function
void voltage_to_hex(float voltage, uint16_t *int_part, uint16_t *frac_part);

#endif /* __ADC_APP_H_ */
