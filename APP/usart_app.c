#include "usart_app.h"
#include "sd_app.h"
#include "flash_app.h"
#include "oled_app.h"
#include "data_storage_app.h"



uint16_t uart_rx_index = 0;
uint32_t uart_rx_ticks = 0;
uint8_t uart_rx_buffer[128] = {0};
uint8_t uart_rx_dma_buffer[128] = {0};
uint8_t uart_dma_buffer[128] = {0};
uint8_t uart_flag = 0;


static uint8_t ratio_state = 0;
static uint8_t limit_state = 0;
static float ratio_backup = 0;
static float limit_backup = 0;

int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
	char buffer[512];
	va_list arg;
	int len;
	// 初始化可变参数列表
	va_start(arg, format);
	len = vsnprintf(buffer, sizeof(buffer), format, arg);
	va_end(arg);
	HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFF);
	return len;
}


// UART初始化函数
void uart_init(void)
{
    my_printf(&huart1, "====system init====\r\n");
	  my_printf(&huart1, "Device_ID:%d\r\n",device_id);
	  my_printf(&huart1, "====system ready====\r\n");
}

// UART DMA 接收初始化函数
void uart_dma_rx_init(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // 1. 确认目标串口 (USART1)
    if (huart->Instance == USART1)
    {
        // 2. 立即停止当前的 DMA 传输 (防止数据冲突)
        //    因为空闲中断意味着发送方已经停止，避免 DMA 等待新数据
        HAL_UART_DMAStop(huart);

        // 3. 从 DMA 接收缓冲区复制有效数据 (Size 字节) 到处理缓冲区
        memcpy(uart_dma_buffer, uart_rx_dma_buffer, Size);
        // 注意：这里使用 Size 只复制实际接收到的数据

        // 4. 设置"数据通知标志"，通知主循环数据处理完成
        uart_flag = 1;

        // 5. 清空 DMA 接收缓冲区，为下次接收做准备
        //    虽然 memcpy 只复制了 Size 字节，但为了安全起见全部清空
        memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));

        // 6. **关键：立即启动下一次 DMA 接收**
        //    如果不再次设置，只能接收一帧数据
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));

        // 7. 之前关闭了半满中断，需要再次关闭 (保险起见)
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}



void system_test(void)
{
	if (strncmp((char *)uart_dma_buffer, "test", 4) == 0)
	{
    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    data_storage_save_log(&dt, "system hardware test");

    my_printf(&huart1, "\r\n====system selftest====\r\n");
    my_printf(&huart1, "flash......ok\r\n");

    my_printf(&huart1, "flash ID:%d\r\n",device_id);

    // 1. 检测 TF 卡
    system_check_status_t sd_status = check_tf_card_status(&sd_info);

    if (sd_status == SYSTEM_CHECK_OK)
    {
        my_printf(&huart1, "TF card......ok\r\n");
        my_printf(&huart1, "TF card memory： %lu KB\r\n", sd_info.capacity_mb * 1024);
        data_storage_save_log(&dt, "test ok");
    }
    else if (sd_status == SYSTEM_CHECK_NOT_FOUND)
    {
        my_printf(&huart1, "TF card......error\r\n");
        my_printf(&huart1, "can not find TF card\r\n");
        data_storage_save_log(&dt, "test error: tf card not found");
    }
    else
    {
        my_printf(&huart1, "ERROR\r\n");
        data_storage_save_log(&dt, "test error: unknown");
    }

    my_printf(&huart1, "====system selftest====\r\n");
	}
}

void uart_config_check(void)
{
	// Skip if it's "config save" or "config read" command (already handled)
	if (strncmp((char *)uart_dma_buffer, "config save", 11) == 0 ||
	    strncmp((char *)uart_dma_buffer, "config read", 11) == 0)
	{
		return;
	}

	if (strncmp((char *)uart_dma_buffer, "conf", 4) == 0)
	{
			config_data_t config;
			system_check_status_t status = read_and_parse_config(&config);

			if (status == SYSTEM_CHECK_NOT_FOUND) {
					my_printf(&huart1, "[CONFIG] ERROR: config.ini not found\r\n");
			} else if (status == SYSTEM_CHECK_OK && config.is_valid) {
					my_printf(&huart1, "Ratio= %.2f\r\n", config.ratio_ch0);
					my_printf(&huart1, "Limit= %.2f\r\n", config.limit_ch0);
					my_printf(&huart1, "config read success\r\n");
					// 记录log
					rtc_datetime_t dt;
					rtc_get_datetime(&dt);
					data_storage_save_log(&dt, "config check (command)");
			} else {
					my_printf(&huart1, "config.ini parse error.\r\n");
			}
	}


}

void uart_ratio_set(void)
{
    if (ratio_state == 1) {
        // 等待输入状态，处理数值
        char *p = (char *)uart_dma_buffer;
        while (*p == ' ') p++;
        uint8_t valid = 0;
        for (char *c = p; *c && *c != '\r' && *c != '\n'; c++) {
            if ((*c >= '0' && *c <= '9') || *c == '.') valid = 1;
            else if (*c != ' ') { valid = 0; break; }
        }
        if (valid) {
            float val = atof(p);
            if (val >= 0 && val <= 100) {
                if (sd_update_ratio(val) == SYSTEM_CHECK_OK) {
                    my_printf(&huart1, "ratio modified success\r\n");
                    my_printf(&huart1, "Ratio=%.2f\r\n", val);
                    // 记录log
                    rtc_datetime_t dt;
                    rtc_get_datetime(&dt);
                    char log_msg[48];
                    snprintf(log_msg, sizeof(log_msg), "ratio config success to %.2f", val);
                    data_storage_save_log(&dt, log_msg);
                } else {
                    my_printf(&huart1, "ratio invalid\r\n");
                    my_printf(&huart1, "Ratio=%.2f\r\n", ratio_backup);
                }
            } else {
                my_printf(&huart1, "ratio invalid\r\n");
                my_printf(&huart1, "Ratio=%.2f\r\n", ratio_backup);
            }
        } else {
            my_printf(&huart1, "ratio invalid\r\n");
            my_printf(&huart1, "Ratio=%.2f\r\n", ratio_backup);
        }
        ratio_state = 0;
        return;
    }

    if (strncmp((char *)uart_dma_buffer, "ratio", 5) == 0) {
        config_data_t config;
        if (read_and_parse_config(&config) == SYSTEM_CHECK_OK && config.is_valid) {
            ratio_backup = config.ratio_ch0;
            my_printf(&huart1, "Ratio=%.2f\r\n", config.ratio_ch0);
            my_printf(&huart1, "Input value(0-100):\r\n");
            ratio_state = 1;
            // 记录log
            rtc_datetime_t dt;
            rtc_get_datetime(&dt);
            data_storage_save_log(&dt, "ratio config");
        } else {
            my_printf(&huart1, "ERROR: Cannot read config.ini\r\n");
        }
    }
}


void uart_limit_set(void)
{
    if (limit_state == 1) {
        // 等待输入状态，处理数值
        char *p = (char *)uart_dma_buffer;
        while (*p == ' ') p++;
        uint8_t valid = 0;
        for (char *c = p; *c && *c != '\r' && *c != '\n'; c++) {
            if ((*c >= '0' && *c <= '9') || *c == '.') valid = 1;
            else if (*c != ' ') { valid = 0; break; }
        }
        if (valid) {
            float val = atof(p);
            if (val >= 0 && val <= 200) {
                if (sd_update_limit(val) == SYSTEM_CHECK_OK) {
                    update_current_limit(val);  // Sync update current_limit
                    my_printf(&huart1, "Limit modified success\r\n");
                    my_printf(&huart1, "Limit=%.2f\r\n", val);
                    // 记录log
                    rtc_datetime_t dt;
                    rtc_get_datetime(&dt);
                    char log_msg[48];
                    snprintf(log_msg, sizeof(log_msg), "limit config success to %.2f", val);
                    data_storage_save_log(&dt, log_msg);
                } else {
                    my_printf(&huart1, "Limit invalid\r\n");
                    my_printf(&huart1, "Limit=%.2f\r\n", limit_backup);
                }
            } else {
                my_printf(&huart1, "Limit invalid\r\n");
                my_printf(&huart1, "Limit=%.2f\r\n", limit_backup);
            }
        } else {
            my_printf(&huart1, "Limit invalid\r\n");
            my_printf(&huart1, "Limit=%.2f\r\n", limit_backup);
        }
        limit_state = 0;
        return;
    }

    if (strncmp((char *)uart_dma_buffer, "limit", 5) == 0) {
        config_data_t config;
        if (read_and_parse_config(&config) == SYSTEM_CHECK_OK && config.is_valid) {
            limit_backup = config.limit_ch0;
            my_printf(&huart1, "Limit=%.2f\r\n", config.limit_ch0);
            my_printf(&huart1, "Input value(0-200):\r\n");
            limit_state = 1;
            // 记录log
            rtc_datetime_t dt;
            rtc_get_datetime(&dt);
            data_storage_save_log(&dt, "limit config");
        } else {
            my_printf(&huart1, "ERROR: Cannot read config.ini\r\n");
        }
    }
}

/**
 * @brief Handle "config save" command - save SD card config to SPI Flash
 */
void uart_config_save(void)
{
    if (strncmp((char *)uart_dma_buffer, "config save", 11) == 0)
    {
        my_printf(&huart1, "Saving config to Flash...\r\n");

        flash_config_status_t status = flash_config_save();

        switch (status)
        {
            case FLASH_CONFIG_OK:
                my_printf(&huart1, "Config saved to Flash!\r\n");
                // 记录log
                {
                    rtc_datetime_t dt;
                    rtc_get_datetime(&dt);
                    data_storage_save_log(&dt, "config save to flash success");
                }
                break;
            case FLASH_CONFIG_READ_SD_ERR:
                my_printf(&huart1, "ERROR: Cannot read config.ini\r\n");
                break;
            case FLASH_CONFIG_INVALID_DATA:
                my_printf(&huart1, "ERROR: Config data invalid\r\n");
                break;
            case FLASH_CONFIG_WRITE_ERR:
                my_printf(&huart1, "ERROR: Flash write failed\r\n");
                break;
            case FLASH_CONFIG_CRC_ERR:
                my_printf(&huart1, "ERROR: CRC verification failed\r\n");
                break;
            default:
                my_printf(&huart1, "ERROR: Unknown error\r\n");
                break;
        }
    }
}

/**
 * @brief Handle "config read" command - read config from SPI Flash
 */
void uart_config_read(void)
{
    if (strncmp((char *)uart_dma_buffer, "config read", 11) == 0)
    {
        flash_config_t config;
        flash_config_status_t status = flash_config_read(&config);

        switch (status)
        {
            case FLASH_CONFIG_OK:
                my_printf(&huart1, "Flash Config:\r\n");
                my_printf(&huart1, "  Ratio= %.2f\r\n", config.ratio_ch0);
                my_printf(&huart1, "  Limit= %.2f\r\n", config.limit_ch0);
						    my_printf(&huart1, "config read success\r\n");
                // 记录log
                {
                    rtc_datetime_t dt;
                    rtc_get_datetime(&dt);
                    data_storage_save_log(&dt, "config read from flash (command)");
                }
                break;
            case FLASH_CONFIG_INVALID_DATA:
                my_printf(&huart1, "ERROR: No valid config in Flash\r\n");
                break;
            case FLASH_CONFIG_CRC_ERR:
                my_printf(&huart1, "ERROR: Flash config CRC error\r\n");
                break;
            default:
                my_printf(&huart1, "ERROR: Unknown error\r\n");
                break;
        }
    }
}

/**
 * @brief Handle "start" and "stop" commands for periodic sampling
 */
void uart_sampling_cmd(void)
{
    if (strncmp((char *)uart_dma_buffer, "start", 5) == 0)
    {
        sampling_start();
        // 记录log
        rtc_datetime_t dt;
        rtc_get_datetime(&dt);
        char log_msg[48];
        snprintf(log_msg, sizeof(log_msg), "sample start - cycle %ds (command)", sample_cycle_sec);
        data_storage_save_log(&dt, log_msg);
    }
    else if (strncmp((char *)uart_dma_buffer, "stop", 4) == 0)
    {
        sampling_stop();
        // 记录log
        rtc_datetime_t dt;
        rtc_get_datetime(&dt);
        data_storage_save_log(&dt, "sample stop (command)");
    }
}

/**
 * @brief Handle "hide" and "unhide" commands
 */
void uart_hide_cmd(void)
{
    if (strcmp((char *)uart_dma_buffer, "hide") == 0)
    {
        set_hide_mode(1);
        // 记录log
        rtc_datetime_t dt;
        rtc_get_datetime(&dt);
        data_storage_save_log(&dt, "hide data");
    }
    else if (strcmp((char *)uart_dma_buffer, "unhide") == 0)
    {
        set_hide_mode(0);
        // 记录log
        rtc_datetime_t dt;
        rtc_get_datetime(&dt);
        data_storage_save_log(&dt, "unhide data");
    }
}

void uart_task(void)
{
	// 如果数据标志为0，说明没有数据需要处理，直接返回
	if (uart_flag == 0)
		return;

	// 先清除标志，允许中断继续接收新数据
	uart_flag = 0;

	// Handle "start" and "stop" commands first
	uart_sampling_cmd();

	// Handle "hide" and "unhide" commands
	uart_hide_cmd();

	system_test();

	// Handle "config save" command (must be before "conf" check!)
	uart_config_save();

	// Handle "config read" command
	uart_config_read();

	// Handle "conf" command
	uart_config_check();

	// ratio命令
	uart_ratio_set();
	uart_limit_set();
	RTC_Handle_Command(uart_dma_buffer);
	// 清空接收缓冲区，为下次接收做准备
	memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
}




