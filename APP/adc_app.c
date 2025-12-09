#include "adc_app.h"
#include "adc.h"
#include "math.h"
#include "dac.h"
#include "tim.h"
#include "usart_app.h"
#include "string.h"
#include "usart.h"
#include "sd_app.h"

// 1 轮询
// 2 DMA循环转换
// 3 DMA TIM 定时采集

// --- 全局变量 ---
#define ADC_DMA_BUFFER_SIZE 32 // DMA 缓冲区大小，可以根据需要调整
uint32_t adc_dma_buffer[ADC_DMA_BUFFER_SIZE]; // DMA 目标缓冲区
__IO uint32_t adc_val;  // 用于存储当前采集的 ADC 值
__IO float voltage;       // 原始电压值 (vo)
__IO float show_voltage;  // 显示电压值 = vo × ratio
static float current_ratio = 1.0f;  // 当前ratio值

// --- 初始化 (通过在 main 函数中或其他初始化函数中的某一次调用) ---
void adc_dma_init(void)
{
    // 启动 ADC 并使用 DMA 传输
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, ADC_DMA_BUFFER_SIZE);

    // 从 SD 卡读取 ratio 值
    config_data_t config;
    if (read_and_parse_config(&config) == SYSTEM_CHECK_OK && config.is_valid)
    {
        current_ratio = config.ratio_ch0;
    }
    else
    {
        current_ratio = 1.0f;  // 默认值
    }
}

// --- 处理任务 (在主循环中或定时器中断中调用) ---
void adc_task(void)
{
    uint32_t adc_sum = 0;

    // 1. 读取 DMA 缓冲区中的所有值并求和
    for(uint16_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++)
    {
        adc_sum += adc_dma_buffer[i];
    }

    // 2. 计算平均 ADC 值
    adc_val = adc_sum / ADC_DMA_BUFFER_SIZE;

    // 3. 将平均值转换为原始电压值 (vo)
    voltage = ((float)adc_val * 3.3f) / 4096.0f;

    // 4. 计算显示电压值 = vo × ratio
    show_voltage = voltage * current_ratio;
}

/**
 * @brief 电压转HEX (整数2字节 + 小数2字节)
 */
void voltage_to_hex(float voltage, uint16_t *int_part, uint16_t *frac_part)
{
    *int_part = (uint16_t)voltage;
    *frac_part = (uint16_t)((voltage - *int_part) * 65536.0f);
}




