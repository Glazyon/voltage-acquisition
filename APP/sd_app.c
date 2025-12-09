#include "sd_app.h"
#include "bsp_driver_sd.h"  // For BSP_SD_Init, BSP_SD_GetCardInfo

sd_card_info_t sd_info;

// External declarations for SD reinitialization
extern SD_HandleTypeDef hsd;
extern void MX_SDIO_SD_Init(void);

/**
 * @brief Ensure FATFS is mounted, mount if not
 * @return 1 if mounted successfully, 0 if failed
 * @note This function checks mount status and mounts only if needed
 */
static uint8_t ensure_fatfs_mounted(void)
{
    // Try to open root directory to check if mounted
    DIR dir;
    FRESULT res = f_opendir(&dir, "0:/");
    if (res == FR_OK)
    {
        f_closedir(&dir);
        return 1;  // Already mounted
    }

    // Not mounted, try to mount
    res = f_mount(&SDFatFS, SDPath, 1);
    if (res == FR_OK)
    {
        my_printf(&huart1, "DEBUG: FATFS mounted by ensure_fatfs_mounted()\r\n");
        return 1;
    }

    my_printf(&huart1, "DEBUG: ensure_fatfs_mounted() failed, res=%d\r\n", res);
    return 0;
}

/**
 * @brief Reinitialize entire SD card stack (SDIO + FATFS)
 * @return SYSTEM_CHECK_OK if successful
 * @note Call this function when file operations fail to recover from errors
 */
system_check_status_t sd_reinit_stack(void)
{
    static uint32_t last_reinit_tick = 0;
    static uint8_t consecutive_failures = 0;
    static uint8_t reinit_in_progress = 0;  // Prevent reentrant calls
    uint32_t current_tick = HAL_GetTick();

    // Prevent reentrant calls
    if (reinit_in_progress)
    {
        my_printf(&huart1, "DEBUG: sd_reinit_stack already in progress, skipping\r\n");
        return SYSTEM_CHECK_ERROR;
    }

    // Cooldown: at least 2 seconds between reinit attempts
    if (current_tick - last_reinit_tick < 2000 && last_reinit_tick != 0)
    {
        my_printf(&huart1, "DEBUG: Reinit cooldown active, skipping\r\n");
        return SYSTEM_CHECK_ERROR;
    }

    // If too many consecutive failures, give up for a while
    if (consecutive_failures >= 3)
    {
        if (current_tick - last_reinit_tick < 10000)  // 10 second lockout
        {
            return SYSTEM_CHECK_ERROR;
        }
        consecutive_failures = 0;  // Reset after lockout period
    }

    last_reinit_tick = current_tick;
    reinit_in_progress = 1;  // Mark as in progress

    // 1. Unmount FATFS to clear all cached state
    f_mount(NULL, SDPath, 0);

    // 2. Wait for hardware to settle (increased from 100ms to 200ms)
    HAL_Delay(200);

    // 3. Deinitialize SDIO hardware
    HAL_SD_DeInit(&hsd);
    HAL_Delay(200);  // Increased from 50ms to 200ms

    // 4. Reinitialize SDIO parameters
    MX_SDIO_SD_Init();
    HAL_Delay(200);  // Increased from 50ms to 200ms

    // 5. Reinitialize SD card through BSP layer (with retry)
    uint8_t retry;
    for (retry = 0; retry < 3; retry++)
    {
        if (BSP_SD_Init() == MSD_OK)
        {
            break;
        }
        HAL_Delay(200);  // Increased from 100ms to 200ms
    }

    if (retry >= 3)
    {
        my_printf(&huart1, "DEBUG: BSP_SD_Init failed after 3 retries\r\n");
        consecutive_failures++;
        reinit_in_progress = 0;  // Clear flag before return
        return SYSTEM_CHECK_ERROR;
    }

    // 6. Remount FATFS
    FRESULT res = f_mount(&SDFatFS, SDPath, 1);
    if (res != FR_OK)
    {
        my_printf(&huart1, "DEBUG: f_mount failed in sd_reinit_stack, res=%d\r\n", res);
        consecutive_failures++;
        reinit_in_progress = 0;  // Clear flag before return
        return SYSTEM_CHECK_ERROR;
    }

    consecutive_failures = 0;  // Reset on success
    reinit_in_progress = 0;  // Clear flag before return
    my_printf(&huart1, "DEBUG: SD stack reinitialized successfully\r\n");
    return SYSTEM_CHECK_OK;
}


static FRESULT get_sd_card_details(uint32_t *capacity_kb, uint32_t *sector_size_out, uint32_t *sector_count_out)
{
    DWORD sector_count = 0;
    WORD sector_size = 0;

    // 1. 获取扇区总数
    if (disk_ioctl(0, GET_SECTOR_COUNT, &sector_count) == RES_OK)
    {
        // 2. 获取扇区大小
        if (disk_ioctl(0, GET_SECTOR_SIZE, &sector_size) == RES_OK)
        {

            *capacity_kb = (uint32_t)((uint64_t)sector_count * sector_size / 1024);
            *sector_size_out = sector_size;
            *sector_count_out = sector_count;
            return RES_OK;
        }
    }
    return RES_ERROR;
}

system_check_status_t check_tf_card_status(sd_card_info_t *sd_info)
{
    if (sd_info == NULL) return SYSTEM_CHECK_ERROR;

    // Use BSP_SD_GetCardInfo - this doesn't reinitialize the card
    // Unlike disk_initialize() or f_getfree(), this only reads cached card info
    HAL_SD_CardInfoTypeDef card_info;

    // Check if SD card is ready by checking card state
    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
    {
        BSP_SD_GetCardInfo(&card_info);

        sd_info->capacity_mb = (uint32_t)(card_info.BlockNbr * card_info.BlockSize / 1024 / 1024);
        sd_info->sector_size = card_info.BlockSize;
        sd_info->sector_count = card_info.BlockNbr;
        sd_info->status = SYSTEM_CHECK_OK;
        return SYSTEM_CHECK_OK;
    }

    sd_info->status = SYSTEM_CHECK_NOT_FOUND;
    sd_info->capacity_mb = 0;
    sd_info->sector_count = 0;
    sd_info->sector_size = 0;

    return SYSTEM_CHECK_NOT_FOUND;
}


// 去除字符串首尾空格
static void trim(char *str)
{
    char *start = str;
    char *end;

    // 去除前导空格
    while (*start == ' ' || *start == '\t') start++;

    // 如果全是空格
    if (*start == 0) {
        str[0] = 0;
        return;
    }

    // 去除尾部空格
    end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    *(end + 1) = 0;

    // 移动字符串到开头
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}


system_check_status_t read_and_parse_config(config_data_t *config)
{
    FRESULT res;
    FIL file;
    char line_buffer[64];
    uint8_t in_ratio_section = 0;
    uint8_t in_limit_section = 0;
    uint8_t found_ratio = 0;
    uint8_t found_limit = 0;

    // 1. 输入验证
    if (config == NULL) {
        return SYSTEM_CHECK_ERROR;
    }

    // 初始化配置结构
    config->ratio_ch0 = 0.0f;
    config->limit_ch0 = 0;
    config->is_valid = 0;

    // 2. Ensure filesystem is mounted (uses helper function to avoid duplicate mounts)
    if (!ensure_fatfs_mounted()) {
        return SYSTEM_CHECK_ERROR;
    }

    // 3. 尝试打开config.ini文件
    res = f_open(&file, "0:/config.ini", FA_READ);
    if (res != FR_OK) {
        return SYSTEM_CHECK_NOT_FOUND;  // 文件不存在
    }

    // 4. 逐行读取并解析INI文件
    while (f_gets(line_buffer, sizeof(line_buffer), &file) != NULL) {
        trim(line_buffer);  // 去除首尾空格

        // 跳过空行和注释
        if (line_buffer[0] == 0 || line_buffer[0] == ';' || line_buffer[0] == '#') {
            continue;
        }

        // 检查是否是section标题
        if (line_buffer[0] == '[') {
            if (strncmp(line_buffer, "[Ratio]", 7) == 0) {
                in_ratio_section = 1;
                in_limit_section = 0;
            } else if (strncmp(line_buffer, "[Limit]", 7) == 0) {
                in_ratio_section = 0;
                in_limit_section = 1;
            } else {
                in_ratio_section = 0;
                in_limit_section = 0;
            }
            continue;
        }

        // 解析key=value
        char *equals = strchr(line_buffer, '=');
        if (equals != NULL) {
            *equals = 0;  // 分割字符串
            char *key = line_buffer;
            char *value = equals + 1;
            trim(key);
            trim(value);

            // [Ratio] section中的Ch0
            if (in_ratio_section && strcmp(key, "Ch0") == 0) {
                config->ratio_ch0 = atof(value);
                found_ratio = 1;
            }
            // [Limit] section中的Ch0
            else if (in_limit_section && strcmp(key, "Ch0") == 0) {
                config->limit_ch0 = atof(value);
                found_limit = 1;
            }
        }
    }

    // 5. 关闭文件
    f_close(&file);

    // 6. 验证是否找到所有必需的配置项
    if (found_ratio && found_limit) {
        config->is_valid = 1;
        return SYSTEM_CHECK_OK;
    }

    return SYSTEM_CHECK_ERROR;  // 配置项不完整
}

/**
 * @brief  Diagnose SD card file system and list root directory
 * @param  None
 * @retval SYSTEM_CHECK_OK if diagnostics successful
 */
system_check_status_t sd_diagnose_filesystem(void)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    uint8_t file_count = 0;

    // Ensure filesystem is mounted (uses helper function to avoid duplicate mounts)
    if (!ensure_fatfs_mounted()) {
        my_printf(&huart1, "[SD] ERROR: Cannot mount filesystem\r\n");
        return SYSTEM_CHECK_ERROR;
    }

    my_printf(&huart1, "[SD] Filesystem ready\r\n");

    // 打开根目录
    res = f_opendir(&dir, "0:/");
    if (res != FR_OK) {
        my_printf(&huart1, "[SD] ERROR: Cannot open root directory (code %d)\r\n", res);
        return SYSTEM_CHECK_ERROR;
    }

    my_printf(&huart1, "[SD] Root directory contents:\r\n");

    // 列出所有文件
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;  // 目录结束

        if (fno.fattrib & AM_DIR) {
            my_printf(&huart1, "  [DIR]  %s\r\n", fno.fname);
        } else {
            my_printf(&huart1, "  [FILE] %s (%lu bytes)\r\n", fno.fname, fno.fsize);
            file_count++;
        }
    }

    f_closedir(&dir);

    if (file_count == 0) {
        my_printf(&huart1, "[SD] WARNING: No files found in root directory\r\n");
        my_printf(&huart1, "[SD] Please create 'config.ini' in SD card root\r\n");
    }

    // 特别检查config.ini
    FILINFO config_info;
    res = f_stat("0:/config.ini", &config_info);
    if (res == FR_OK) {
        my_printf(&huart1, "[SD] Found: config.ini (%lu bytes)\r\n", config_info.fsize);
    } else {
        my_printf(&huart1, "[SD] NOT FOUND: config.ini (error code %d)\r\n", res);
        my_printf(&huart1, "[SD] Hint: Use 'sd create' to generate sample\r\n");
    }

    return SYSTEM_CHECK_OK;
}

/**
 * @brief  Create sample config.ini file on SD card
 * @param  None
 * @retval SYSTEM_CHECK_OK if file created successfully
 */
system_check_status_t sd_create_sample_config(void)
{
    FRESULT res;
    FIL file;
    UINT bytes_written;

    const char* sample_content =
        "; Sample configuration file\r\n"
        "; Edit values and save to SD card\r\n"
        "\r\n"
        "[Ratio]\r\n"
        "Ch0 = 1.0\r\n"
        "\r\n"
        "[Limit]\r\n"
        "Ch0 = 200.0\r\n";

    // Ensure filesystem is mounted (uses helper function to avoid duplicate mounts)
    if (!ensure_fatfs_mounted()) {
        my_printf(&huart1, "[SD] ERROR: Cannot mount filesystem\r\n");
        return SYSTEM_CHECK_ERROR;
    }

    // 创建/覆盖config.ini
    res = f_open(&file, "0:/config.ini", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        my_printf(&huart1, "[SD] ERROR: Cannot create config.ini (code %d)\r\n", res);
        return SYSTEM_CHECK_ERROR;
    }

    // 写入样例内容
    res = f_write(&file, sample_content, strlen(sample_content), &bytes_written);
    if (res != FR_OK || bytes_written != strlen(sample_content)) {
        my_printf(&huart1, "[SD] ERROR: Write failed\r\n");
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    f_close(&file);

    my_printf(&huart1, "[SD] SUCCESS: Created config.ini (%u bytes)\r\n", bytes_written);
    my_printf(&huart1, "[SD] File contains default configuration\r\n");

    return SYSTEM_CHECK_OK;
}

system_check_status_t sd_update_ratio(float new_ratio)
{
    FRESULT res;
    FIL file;
    config_data_t config;
    UINT bw;

    // 1. 读取当前配置
    system_check_status_t status = read_and_parse_config(&config);
    if (status != SYSTEM_CHECK_OK) {
        return status;
    }

    // 2. 修改ratio字段
    config.ratio_ch0 = new_ratio;

    // 3. 生成新配置内容
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer),
                       "; Configuration file\r\n"
                       "\r\n"
                       "[Ratio]\r\n"
                       "Ch0 = %.1f\r\n"
                       "\r\n"
                       "[Limit]\r\n"
                       "Ch0 = %.1f\r\n",
                       config.ratio_ch0, config.limit_ch0);

    if (len < 0 || len >= sizeof(buffer)) {
        return SYSTEM_CHECK_ERROR;
    }

    // 4. 打开文件（不清空！）
    res = f_open(&file, "0:/config.ini", FA_OPEN_EXISTING | FA_WRITE);
    if (res == FR_NO_FILE) {
        // 文件不存在，创建新文件
        res = f_open(&file, "0:/config.ini", FA_CREATE_NEW | FA_WRITE);
    }
    if (res != FR_OK) {
        return SYSTEM_CHECK_ERROR;
    }

    // 5. 定位到文件开头
    res = f_lseek(&file, 0);
    if (res != FR_OK) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 6. 写入新数据
    res = f_write(&file, buffer, len, &bw);
    if (res != FR_OK || bw != (UINT)len) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 7. 截断文件到新长度（移除旧的多余数据）
    res = f_truncate(&file);
    if (res != FR_OK) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 8. 强制刷新到物理介质
    res = f_sync(&file);
    f_close(&file);

    return (res == FR_OK) ? SYSTEM_CHECK_OK : SYSTEM_CHECK_ERROR;
}

system_check_status_t sd_update_limit(float new_limit)
{
    FRESULT res;
    FIL file;
    config_data_t config;
    UINT bw;

    // 1. 读取当前配置
    system_check_status_t status = read_and_parse_config(&config);
    if (status != SYSTEM_CHECK_OK) {
        return status;
    }

    // 2. 修改limit字段
    config.limit_ch0 = new_limit;

    // 3. 生成新配置内容
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer),
                       "; Configuration file\r\n"
                       "\r\n"
                       "[Ratio]\r\n"
                       "Ch0 = %.1f\r\n"
                       "\r\n"
                       "[Limit]\r\n"
                       "Ch0 = %.1f\r\n",
                       config.ratio_ch0, config.limit_ch0);

    if (len < 0 || len >= sizeof(buffer)) {
        return SYSTEM_CHECK_ERROR;
    }

    // 4. 打开文件（不清空！）
    res = f_open(&file, "0:/config.ini", FA_OPEN_EXISTING | FA_WRITE);
    if (res == FR_NO_FILE) {
        // 文件不存在，创建新文件
        res = f_open(&file, "0:/config.ini", FA_CREATE_NEW | FA_WRITE);
    }
    if (res != FR_OK) {
        return SYSTEM_CHECK_ERROR;
    }

    // 5. 定位到文件开头
    res = f_lseek(&file, 0);
    if (res != FR_OK) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 6. 写入新数据
    res = f_write(&file, buffer, len, &bw);
    if (res != FR_OK || bw != (UINT)len) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 7. 截断文件到新长度（移除旧的多余数据）
    res = f_truncate(&file);
    if (res != FR_OK) {
        f_close(&file);
        return SYSTEM_CHECK_ERROR;
    }

    // 8. 强制刷新到物理介质
    res = f_sync(&file);
    f_close(&file);

    return (res == FR_OK) ? SYSTEM_CHECK_OK : SYSTEM_CHECK_ERROR;
}



