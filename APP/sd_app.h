#ifndef __SD_APP_H__
#define __SD_APP_H__

#include "mydefine.h"

typedef enum 
{
    SYSTEM_CHECK_OK = 0,       
    SYSTEM_CHECK_ERROR = 1,    
    SYSTEM_CHECK_NOT_FOUND = 2 
} system_check_status_t;


typedef struct
{
    uint32_t capacity_mb;
    uint32_t sector_count;
    uint16_t sector_size;
    system_check_status_t status;
} sd_card_info_t;

// 配置文件数据结构
typedef struct
{
    float ratio_ch0;       // [Ratio] Ch0值
    float limit_ch0;       // [Limit] Ch0值（浮点数，上限200）
    uint8_t is_valid;      // 配置是否有效 (1=有效, 0=无效)
} config_data_t;



 extern sd_card_info_t sd_info;   




static FRESULT get_sd_card_details(uint32_t *capacity_kb, uint32_t *sector_size_out, uint32_t *sector_count_out);
system_check_status_t check_tf_card_status(sd_card_info_t *sd_info);

// 读取并解析config.ini文件
system_check_status_t read_and_parse_config(config_data_t *config);

// SD卡诊断和辅助功能
system_check_status_t sd_diagnose_filesystem(void);
system_check_status_t sd_create_sample_config(void);
system_check_status_t sd_update_ratio(float new_ratio);
system_check_status_t sd_update_limit(float new_limit);

// SD卡栈重新初始化（用于错误恢复）
system_check_status_t sd_reinit_stack(void);






#endif

