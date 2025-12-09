#include "oled_app.h"
#include "oled.h"
#include "rtc_app.h"
#include "flash_app.h"
#include "usart_app.h"
#include "sd_app.h"
#include "adc_app.h"
#include "data_storage_app.h"

// 外部变量
extern uint8_t ucLed[6];           // LED状态数组，来自led_app.c
extern __IO float show_voltage;    // 显示电压 = vo × ratio，来自adc_app.c

// 采样状态变量
uint8_t sampling_running = 0;     // 0=停止, 1=运行
uint8_t sample_cycle_sec = 5;     // 默认5秒

// 私有变量
static uint32_t last_sample_tick = 0;   // 上次采样时间戳
static uint32_t last_led_tick = 0;      // LED闪烁时间戳
static uint8_t oled_initialized = 0;    // OLED初始化标志
static float current_limit = 0;         // 当前限值阈值
static uint8_t hide_mode = 0;           // 0=正常模式, 1=hide模式

/**
 * @brief 设置hide模式
 */
void set_hide_mode(uint8_t mode)
{
    hide_mode = mode;
}

/**
 * @brief 更新当前限值（通过串口修改limit时调用）
 * @param new_limit 新的限值
 */
void update_current_limit(float new_limit)
{
    current_limit = new_limit;
}

/**
 * @brief 启动周期采样
 */
void sampling_start(void)
{
    if (!sampling_running)
    {
        sampling_running = 1;
        last_sample_tick = HAL_GetTick();
        last_led_tick = HAL_GetTick();

        // 从SD卡配置文件读取限值
        config_data_t config;
        if (read_and_parse_config(&config) == SYSTEM_CHECK_OK && config.is_valid)
        {
            current_limit = config.limit_ch0;
        }
        else
        {
            current_limit = 100.0f;  // 未配置时的默认限值
        }

        OLED_Clear();  // 进入采样模式时清屏一次
        my_printf(&huart1, "Periodic Sampling\r\n");
        my_printf(&huart1, "sample cycle:%ds\r\n", sample_cycle_sec);
    }
}

/**
 * @brief 停止周期采样
 */
void sampling_stop(void)
{
    if (sampling_running)
    {
        sampling_running = 0;
        ucLed[0] = 0;  // LED1关闭
        ucLed[1] = 0;  // LED2关闭
        OLED_Clear();  // 显示空闲状态前清屏
        my_printf(&huart1, "Periodic Sampling STOP\r\n");
    }
}

/**
 * @brief 切换采样状态
 */
void sampling_toggle(void)
{
    if (sampling_running)
        sampling_stop();
    else
        sampling_start();
}

/**
 * @brief 设置采样周期
 * @param sec 周期秒数 (5/10/15)
 */
void sampling_set_cycle(uint8_t sec)
{
    sample_cycle_sec = sec;
    my_printf(&huart1, "sample cycle adjust:%ds\r\n", sec);
}

/**
 * @brief OLED显示任务（含周期采样）
 */
void oled_task(void)
{
    // 首次运行时初始化OLED
    if (!oled_initialized)
    {
        OLED_Init();
        OLED_Clear();
        oled_initialized = 1;
    }

    uint32_t now = HAL_GetTick();

    if (sampling_running)
    {
        // LED1闪烁（1秒周期 = 500ms亮/灭）
        if (now - last_led_tick >= 500)
        {
            last_led_tick = now;
            ucLed[0] ^= 1;  // 切换LED1
        }

        // 获取当前时间
        rtc_datetime_t dt;
        rtc_get_datetime(&dt);

        // 获取电压（使用 show_voltage = vo × ratio）
        float volt = show_voltage;

        // 检查是否超限
        uint8_t over_limit = (volt > current_limit);
        ucLed[1] = over_limit;  // LED2指示超限

        // OLED实时刷新（字体12）- 不清屏避免闪烁
        char line1[17], line2[17];
        snprintf(line1, sizeof(line1), "%02d:%02d:%02d  ",
                 dt.hours, dt.minutes, dt.seconds);
        snprintf(line2, sizeof(line2), "%.2f V    ", volt);
        OLED_ShowStr(0, 0, line1, 12);  // 第1行：时间
        OLED_ShowStr(0, 2, line2, 12);  // 第2行：电压

        // 周期性串口输出
        if (now - last_sample_tick >= (uint32_t)sample_cycle_sec * 1000)
        {
            last_sample_tick = now;

            // ========== 数据存储 ==========
            // 同步hide模式到存储模块
            data_storage_set_hide_mode(hide_mode);

            // 1. 存储采样数据（非hide模式）
            if (!hide_mode)
            {
                data_storage_save_sample(&dt, volt);
            }

            // 2. 如果超限，存储overLimit数据（无论何种模式）
            if (over_limit)
            {
                data_storage_save_overlimit(&dt, volt, current_limit);
            }

            // 3. 如果是hide模式，存储hideData
            if (hide_mode)
            {
                data_storage_save_hidedata(&dt, volt, over_limit);
            }

            // ========== 串口输出 ==========
            if (hide_mode)
            {
                // HEX格式输出
                uint32_t unix_ts = datetime_to_unix(&dt);
                uint16_t v_int, v_frac;
                voltage_to_hex(volt, &v_int, &v_frac);
                my_printf(&huart1, "%08X%04X%04X%s\r\n", unix_ts, v_int, v_frac, over_limit ? "*" : "");
            }
            else if (over_limit)
            {
                my_printf(&huart1, "%04d-%02d-%02d %02d:%02d:%02d ch0=%.2fV OverLimit(%.2f)\r\n",
                    dt.year, dt.month, dt.date,
                    dt.hours, dt.minutes, dt.seconds,
                    volt, current_limit);
            }
            else
            {
                my_printf(&huart1, "%04d-%02d-%02d %02d:%02d:%02d ch0=%.2fV\r\n",
                    dt.year, dt.month, dt.date,
                    dt.hours, dt.minutes, dt.seconds, volt);
            }
        }
    }
    else
    {
        // 空闲状态显示
        OLED_ShowStr(0, 0, "system idle", 12);
    }
}
