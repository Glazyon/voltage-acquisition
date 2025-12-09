#ifndef __USART_APP_H__
#define __USART_APP_H__

#include "mydefine.h"


int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void uart_init(void);
void uart_dma_rx_init(void);
void uart_task(void);
void system_test(void);
void uart_config_check(void);
void uart_limit_set(void);



#endif

