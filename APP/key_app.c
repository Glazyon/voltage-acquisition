#include "key_app.h"
#include "sd_app.h"
#include "usart_app.h"
#include "oled_app.h"
#include "data_storage_app.h"


uint8_t key_val = 0, key_old = 0, key_down = 0, key_up = 0;

uint8_t key_read()
{
	uint8_t temp = 0;

	if(HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET) temp = 1;
	if(HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_RESET) temp = 2;
	if(HAL_GPIO_ReadPin(KEY_3_GPIO_Port, KEY_3_Pin) == GPIO_PIN_RESET) temp = 3;
	if(HAL_GPIO_ReadPin(KEY_4_GPIO_Port, KEY_4_Pin) == GPIO_PIN_RESET) temp = 4;
	if(HAL_GPIO_ReadPin(KEY_5_GPIO_Port, KEY_5_Pin) == GPIO_PIN_RESET) temp = 5;
	if(HAL_GPIO_ReadPin(KEY_6_GPIO_Port, KEY_6_Pin) == GPIO_PIN_RESET) temp = 6;

	return temp;
}

void key_task()
{
	key_val = key_read();

	// Detect new key press (value changed and key is pressed)
	if(key_val != 0 && key_val != key_old)
	{
		key_down = key_val;
	}
	else
	{
		key_down = 0;
	}
	key_old = key_val;

	if(key_down == 1)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		ucLed[0]^= 1;  // KEY1: Toggle LED1
	}
	else if(key_down == 2)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		// KEY2: Toggle sampling state
		uint8_t was_running = sampling_running;
		sampling_toggle();
		// 记录log
		rtc_datetime_t dt;
		rtc_get_datetime(&dt);
		if (!was_running) {
			char log_msg[48];
			snprintf(log_msg, sizeof(log_msg), "sample start - cycle %ds (key press)", sample_cycle_sec);
			data_storage_save_log(&dt, log_msg);
		} else {
			data_storage_save_log(&dt, "sample stop (key press)");
		}
	}
	else if(key_down == 3)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		sampling_set_cycle(5);  // KEY3: Set cycle to 5s
		// 记录log
		rtc_datetime_t dt;
		rtc_get_datetime(&dt);
		data_storage_save_log(&dt, "cycle switch to 5s (key press)");
	}
	else if(key_down == 4)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		sampling_set_cycle(10);  // KEY4: Set cycle to 10s
		// 记录log
		rtc_datetime_t dt;
		rtc_get_datetime(&dt);
		data_storage_save_log(&dt, "cycle switch to 10s (key press)");
	}
	else if(key_down == 5)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		sampling_set_cycle(15);  // KEY5: Set cycle to 15s
		// 记录log
		rtc_datetime_t dt;
		rtc_get_datetime(&dt);
		data_storage_save_log(&dt, "cycle switch to 15s (key press)");
	}
	else if(key_down == 6)
	{
		if(key_slow_down < 50) return;
		key_slow_down = 0;
		ucLed[5]^= 1;  // KEY6: Toggle LED6
	}
}


