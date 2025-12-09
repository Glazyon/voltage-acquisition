#include "scheduler.h"
#include "mydefine.h"

// 全局变量，用于存储任务数量
uint8_t task_num;

typedef struct {
    void (*task_func)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;



// 静态任务数组，每个任务包含：函数指针、执行间隔（毫秒）、上次运行时间（毫秒）
static task_t scheduler_task[] =
{
  {led_task,100,0},
  {key_task,10,0},
  {uart_task,5,0},
  {oled_task,200,0},
  {adc_task,5,0}
};

/**
 * @brief 任务调度器初始化函数
 * 计算任务数组元素个数，并存储到 task_num 中
 */
void scheduler_init(void)
{
    // 计算任务数组元素个数，并存储到 task_num 中
    task_num = sizeof(scheduler_task) / sizeof(task_t);
}

/**
 * @brief 任务调度器运行函数
 * 遍历任务数组，检查是否有任务需要执行。如果当前时间已经超过任务的执行间隔，则执行该任务并更新上次执行时间
 */
void scheduler_run(void)
{
    // 遍历任务数组中的所有任务
    for (uint8_t i = 0; i < task_num; i++)
    {
        // 获取当前的系统时间（毫秒）
        uint32_t now_time = HAL_GetTick();

        // 检查当前时间是否达到该任务的执行时间
        if (now_time >= scheduler_task[i].rate_ms + scheduler_task[i].last_run)
        {
            // 更新任务的上次执行时间为当前时间
            scheduler_task[i].last_run = now_time;

            // 执行任务函数
            scheduler_task[i].task_func();
        }
    }
}


