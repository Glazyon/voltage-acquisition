#include "led_app.h"
#include "gpio.h" // 确保已包含HAL库的GPIO头文件

uint8_t ucLed[6] = {1,0,1,0,1,1};  // LED 状态数组 (6个LED)

/**
 * @brief 根据ucLed数组状态控制6个LED显示
 * @param ucLed LED数据存储数组 (大小为6)
 */
void led_disp(uint8_t *ucLed)
{
    uint8_t temp = 0x00;                // 用于记录当前 LED 状态的临时变量 (只用6位有效)
    static uint8_t temp_old = 0xff;     // 记录之前 LED 状态的变化，用于判断是否需要更新显示

    for (int i = 0; i < 6; i++)         // 遍历6个LED的状态
    {
        // 将LED状态整合到temp变量中，进行位运算处理
        if (ucLed[i]) temp |= (1 << i); // 如果ucLed[i]为1，将temp的第i位置1
    }

    // 只有当前状态与之前状态不同时，才更新
    if (temp != temp_old)
    {
        // 使用HAL库函数根据temp值设置对应状态 (高低电平)
        HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, (temp & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 0
        HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, (temp & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 1
//        HAL_GPIO_WritePin(LED_3_GPIO_Port, LED_3_Pin, (temp & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 2
//        HAL_GPIO_WritePin(LED_4_GPIO_Port, LED_4_Pin, (temp & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 3
//        HAL_GPIO_WritePin(LED_5_GPIO_Port, LED_5_Pin, (temp & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 4
//        HAL_GPIO_WritePin(LED_6_GPIO_Port, LED_6_Pin, (temp & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 5

        temp_old = temp;                // 记录新的状态
    }
}

/**
 * @brief LED 显示 (循环)
 */
void led_task(void)
{
    led_disp(ucLed);                    // led_disp显示LED状态
}

//void led_task(void)
//{
//    // 定义一个静态变量来跟踪当前点亮的LED索引
//    static int current_led_index = 0;
//    // 定义一个静态变量来保存上次流水灯的时间戳
//    static uint32_t last_chase_time = 0;
//    // 定义流水灯的速度，例如，每500ms移动一个LED位置
//    const uint32_t CHASE_INTERVAL_MS = 100; // 流水灯间隔时间（毫秒）

//    // 获取当前系统时间（一般来说HAL_GetTick()提供毫秒级系统时间）
//    uint32_t current_time = HAL_GetTick(); // 调用HAL_GetTick()获取当前时间戳

//    // 判断是否达到流水灯的时间间隔
//    if ((current_time - last_chase_time) >= CHASE_INTERVAL_MS)
//    {
//        // 更新上次时间戳
//        last_chase_time = current_time;

//        // 1. 清除LED状态数组（全部熄灭）
//        for (int i = 0; i < 6; i++)
//        {
//            ucLed[i] = 0;
//        }

//        // 2. 设置当前需要点亮的LED位置为1
//        ucLed[current_led_index] = 1;

//        // 3. 准备下一个需要点亮的LED位置
//        current_led_index++;
//        if (current_led_index >= 6)
//        {
//            current_led_index = 0; // 如果到达最后一个LED，则回到第一个LED
//        }
//    }

//    // 调用 led_disp 函数，根据 ucLed 数组状态显示实际的LED显示
//    led_disp(ucLed);
//}
