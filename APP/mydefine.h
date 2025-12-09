#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"

#include "main.h"

#include "usart.h"
#include "adc.h"
#include "tim.h"
#include "dac.h"



#include "scheduler.h"
#include "ringbuffer.h"
#include "oled.h"
#include "lfs.h"
#include "lfs_port.h"
#include "gd25qxx.h"
#include "ff.h"    
#include "fatfs.h" 

#include "adc_app.h"
#include "led_app.h"
#include "usart_app.h"
#include "key_app.h"
#include "flash_app.h"
#include "sd_app.h"
#include "oled_app.h"
#include "rtc_app.h"




extern uint16_t uart_rx_index;
extern uint32_t uart_rx_ticks;
extern uint8_t uart_rx_buffer[128];
extern uint8_t uart_rx_dma_buffer[128];
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern struct rt_ringbuffer uart_ringbuffer;
extern uint8_t ringbuffer_pool[128];
extern uint8_t ucLed[6];

extern uint8_t retSD;  
extern char SDPath[4]; 
extern FATFS SDFatFS;  
extern FIL SDFile; 


