// data_storage_app.c
// Data storage module implementation
// Supports TF card file storage and data format management
// Copyright (c) 2024. All rights reserved.

#include "data_storage_app.h"
#include "fatfs.h"     // FATFS file system
#include "rtc_app.h"   // RTC time application
#include "usart_app.h" // UART application
#include "sd_app.h"    // SD card utilities (for sd_reinit_stack)
#include "string.h"    // String operations
#include "stdio.h"     // Standard I/O

// ============================================================================
// External function declarations
// ============================================================================
extern uint32_t datetime_to_unix(rtc_datetime_t *dt);  // Defined in rtc_app.c

// ============================================================================
// Static variables
// ============================================================================
static file_state_t g_file_states[STORAGE_TYPE_COUNT]; // File state array
static uint32_t g_boot_count = 0;                      // Boot counter
static uint8_t g_hide_mode = 0;                        // Hide mode flag

// Directory names array
static const char *g_directory_names[STORAGE_TYPE_COUNT] = {
    "sample",    // STORAGE_SAMPLE
    "overLimit", // STORAGE_OVERLIMIT
    "log",       // STORAGE_LOG
    "hideData"   // STORAGE_HIDEDATA
};

// Filename prefix array
static const char *g_filename_prefixes[STORAGE_TYPE_COUNT] = {
    "sampleData", // STORAGE_SAMPLE
    "overLimit",  // STORAGE_OVERLIMIT
    "log",        // STORAGE_LOG
    "hideData"    // STORAGE_HIDEDATA
};

// ============================================================================
// Private function declarations
// ============================================================================
static uint32_t get_boot_count_from_fatfs(void);
static data_storage_status_t save_boot_count_to_fatfs(uint32_t boot_count);
static data_storage_status_t create_storage_directories(void);
static data_storage_status_t check_and_update_filename(storage_type_t type);
static data_storage_status_t write_data_to_file(storage_type_t type, const char *data);
static data_storage_status_t format_sample_data(float voltage, char *formatted_data);
static data_storage_status_t format_overlimit_data(float voltage, float limit, char *formatted_data);
static data_storage_status_t format_log_data(const char *operation, char *formatted_data);
static data_storage_status_t format_hidedata(float voltage, uint8_t is_overlimit, char *formatted_data);
static void format_hex_output(uint32_t timestamp, float voltage, uint8_t is_overlimit, char *output);

// ============================================================================
// Boot count management (using SD card file)
// ============================================================================

/**
 * @brief Read boot count from FATFS
 * @return Boot count value, 0 if failed
 */
static uint32_t get_boot_count_from_fatfs(void)
{
    FIL file;
    uint32_t boot_count = 0;
    UINT bytes_read;
    FRESULT res;

    res = f_open(&file, "0:/boot_count.txt", FA_READ);
    if (res == FR_OK)
    {
        res = f_read(&file, &boot_count, sizeof(boot_count), &bytes_read);
        if (res != FR_OK || bytes_read != sizeof(boot_count))
        {
            boot_count = 0;
        }
        f_close(&file);
    }
    // If file doesn't exist, boot_count remains 0

    return boot_count;
}

/**
 * @brief Save boot count to FATFS
 * @param boot_count Boot count value to save
 * @return Operation status
 */
static data_storage_status_t save_boot_count_to_fatfs(uint32_t boot_count)
{
    FIL file;
    UINT bytes_written;
    FRESULT res;

    res = f_open(&file, "0:/boot_count.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        return DATA_STORAGE_ERROR;
    }

    res = f_write(&file, &boot_count, sizeof(boot_count), &bytes_written);
    if (res != FR_OK || bytes_written != sizeof(boot_count))
    {
        f_close(&file);
        return DATA_STORAGE_ERROR;
    }

    f_sync(&file);
    f_close(&file);
    return DATA_STORAGE_OK;
}

// ============================================================================
// Directory management
// ============================================================================

/**
 * @brief Create storage directories
 * @return Operation status
 */
static data_storage_status_t create_storage_directories(void)
{
    FRESULT res;
    uint8_t success_count = 0;

    for (uint8_t i = 0; i < STORAGE_TYPE_COUNT; i++)
    {
        char dir_path[32];
        sprintf(dir_path, "0:/%s", g_directory_names[i]);
        res = f_mkdir(dir_path);
        if (res == FR_OK)
        {
            my_printf(&huart1, "Created directory: %s\r\n", g_directory_names[i]);
            success_count++;
        }
        else if (res == FR_EXIST)
        {
            // Directory already exists, that's fine
            success_count++;
        }
        else
        {
            my_printf(&huart1, "Failed to create directory %s, error: %d\r\n", g_directory_names[i], res);
        }
    }

    return (success_count == STORAGE_TYPE_COUNT) ? DATA_STORAGE_OK : DATA_STORAGE_ERROR;
}

// ============================================================================
// Filename generation
// ============================================================================

/**
 * @brief Generate datetime string (YYYYMMDDHHmmss format, 14 digits)
 * @param datetime_str Output buffer (at least 15 bytes)
 * @return Operation status
 */
data_storage_status_t generate_datetime_string(char *datetime_str)
{
    if (datetime_str == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    sprintf(datetime_str, "%04d%02d%02d%02d%02d%02d",
            dt.year,
            dt.month,
            dt.date,
            dt.hours,
            dt.minutes,
            dt.seconds);

    return DATA_STORAGE_OK;
}

/**
 * @brief Generate filename for specified storage type
 * @param type Storage type
 * @param filename Output buffer (at least 64 bytes)
 * @return Operation status
 */
data_storage_status_t generate_filename(storage_type_t type, char *filename)
{
    if (filename == NULL || type >= STORAGE_TYPE_COUNT)
    {
        return DATA_STORAGE_INVALID;
    }

    if (type == STORAGE_LOG)
    {
        // Log files use boot count: log{boot_count}.txt
        sprintf(filename, "%s%lu.txt", g_filename_prefixes[type], g_boot_count);
    }
    else
    {
        // Other files use datetime: {prefix}{datetime}.txt
        char datetime_str[16];
        data_storage_status_t result = generate_datetime_string(datetime_str);
        if (result != DATA_STORAGE_OK)
        {
            return result;
        }
        sprintf(filename, "%s%s.txt", g_filename_prefixes[type], datetime_str);
    }

    return DATA_STORAGE_OK;
}

// ============================================================================
// File management
// ============================================================================

/**
 * @brief Check and update filename (create new file every 10 records)
 * @param type Storage type
 * @return Operation status
 */
static data_storage_status_t check_and_update_filename(storage_type_t type)
{
    if (type >= STORAGE_TYPE_COUNT)
    {
        return DATA_STORAGE_INVALID;
    }

    file_state_t *state = &g_file_states[type];

    // Create new file if count >= 10 or file doesn't exist yet
    if (state->data_count >= 10 || !state->file_exists)
    {
        char filename[64];
        data_storage_status_t result = generate_filename(type, filename);
        if (result != DATA_STORAGE_OK)
        {
            return result;
        }

        strcpy(state->current_filename, filename);
        state->data_count = 0;
        state->file_exists = 1;

        // my_printf(&huart1, "DEBUG: New file prepared, type=%d, filename=%s\r\n", type, filename);
    }

    return DATA_STORAGE_OK;
}

/**
 * @brief Write data to file
 * @param type Storage type
 * @param data Data string to write
 * @return Operation status
 */
static data_storage_status_t write_data_to_file(storage_type_t type, const char *data)
{
    if (type >= STORAGE_TYPE_COUNT || data == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    data_storage_status_t result = check_and_update_filename(type);
    if (result != DATA_STORAGE_OK)
    {
        return result;
    }

    file_state_t *state = &g_file_states[type];

    // Build full path (must include "0:/" prefix for FATFS)
    char full_path[96];
    sprintf(full_path, "0:/%s/%s", g_directory_names[type], state->current_filename);

    // Try to open file
    FIL file_handle;
    FRESULT res = f_open(&file_handle, full_path, FA_OPEN_ALWAYS | FA_WRITE);

    // If failed, try to reinitialize SD stack and retry ONCE
    if (res != FR_OK)
    {
        my_printf(&huart1, "DEBUG: File open failed (res=%d), reinitializing SD stack...\r\n", res);

        if (sd_reinit_stack() == SYSTEM_CHECK_OK)
        {
            // Retry after reinit
            res = f_open(&file_handle, full_path, FA_OPEN_ALWAYS | FA_WRITE);
        }

        if (res != FR_OK)
        {
            my_printf(&huart1, "DEBUG: File open still failed after reinit, path=%s, res=%d\r\n", full_path, res);
            return DATA_STORAGE_ERROR;
        }
        my_printf(&huart1, "DEBUG: File open succeeded after SD stack reinit\r\n");
    }

    // Seek to end of file (append mode)
    res = f_lseek(&file_handle, f_size(&file_handle));
    if (res != FR_OK)
    {
        f_close(&file_handle);
        return DATA_STORAGE_ERROR;
    }

    // Write data
    UINT bytes_written;
    res = f_write(&file_handle, data, strlen(data), &bytes_written);
    if (res != FR_OK || bytes_written != strlen(data))
    {
        my_printf(&huart1, "DEBUG: File write failed, type=%d, res=%d, expected=%d, written=%d\r\n",
                  type, res, strlen(data), bytes_written);
        f_close(&file_handle);
        return DATA_STORAGE_ERROR;
    }

    // Write newline
    res = f_write(&file_handle, "\n", 1, &bytes_written);
    if (res != FR_OK || bytes_written != 1)
    {
        f_close(&file_handle);
        return DATA_STORAGE_ERROR;
    }

    // Sync and close
    f_sync(&file_handle);
    f_close(&file_handle);

    // Increment data count
    state->data_count++;

    return DATA_STORAGE_OK;
}

// ============================================================================
// Data formatting functions
// ============================================================================

/**
 * @brief Format sample data
 * @param voltage Voltage value
 * @param formatted_data Output buffer
 * @return Operation status
 */
static data_storage_status_t format_sample_data(float voltage, char *formatted_data)
{
    if (formatted_data == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    sprintf(formatted_data, "%04d-%02d-%02d %02d:%02d:%02d %.1fV",
            dt.year,
            dt.month,
            dt.date,
            dt.hours,
            dt.minutes,
            dt.seconds,
            voltage);

    return DATA_STORAGE_OK;
}

/**
 * @brief Format over-limit data
 * @param voltage Voltage value
 * @param limit Limit threshold
 * @param formatted_data Output buffer
 * @return Operation status
 */
static data_storage_status_t format_overlimit_data(float voltage, float limit, char *formatted_data)
{
    if (formatted_data == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    sprintf(formatted_data, "%04d-%02d-%02d %02d:%02d:%02d %.0fV limit %.0fV",
            dt.year,
            dt.month,
            dt.date,
            dt.hours,
            dt.minutes,
            dt.seconds,
            voltage,
            limit);

    return DATA_STORAGE_OK;
}

/**
 * @brief Format log data
 * @param operation Operation description
 * @param formatted_data Output buffer
 * @return Operation status
 */
static data_storage_status_t format_log_data(const char *operation, char *formatted_data)
{
    if (formatted_data == NULL || operation == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    sprintf(formatted_data, "%04d-%02d-%02d %02d:%02d:%02d %s",
            dt.year,
            dt.month,
            dt.date,
            dt.hours,
            dt.minutes,
            dt.seconds,
            operation);

    return DATA_STORAGE_OK;
}

/**
 * @brief Format hex output string
 * @param timestamp Unix timestamp
 * @param voltage Voltage value
 * @param is_overlimit Over-limit flag
 * @param output Output buffer (at least 32 bytes)
 */
static void format_hex_output(uint32_t timestamp, float voltage, uint8_t is_overlimit, char *output)
{
    uint16_t v_int = (uint16_t)voltage;
    uint16_t v_frac = (uint16_t)((voltage - v_int) * 10);  // 1 decimal place
    sprintf(output, "%08X%04X%04X%s", (unsigned int)timestamp, v_int, v_frac, is_overlimit ? "*" : "");
}

/**
 * @brief Format hidden data (original + hex format)
 * @param voltage Voltage value
 * @param is_overlimit Over-limit flag
 * @param formatted_data Output buffer
 * @return Operation status
 */
static data_storage_status_t format_hidedata(float voltage, uint8_t is_overlimit, char *formatted_data)
{
    if (formatted_data == NULL)
    {
        return DATA_STORAGE_INVALID;
    }

    rtc_datetime_t dt;
    rtc_get_datetime(&dt);

    // Format original line
    char original_line[128];
    sprintf(original_line, "%04d-%02d-%02d %02d:%02d:%02d %.1fV",
            dt.year,
            dt.month,
            dt.date,
            dt.hours,
            dt.minutes,
            dt.seconds,
            voltage);

    // Format hex output
    uint32_t timestamp = datetime_to_unix(&dt);
    char hex_output[32];
    format_hex_output(timestamp, voltage, is_overlimit, hex_output);

    // Combine both formats
    sprintf(formatted_data, "%s\nhide: %s", original_line, hex_output);

    return DATA_STORAGE_OK;
}

// ============================================================================
// Public API implementation - Initialization
// ============================================================================

/**
 * @brief Initialize storage system
 * @return Operation status
 */
data_storage_status_t data_storage_init(void)
{
    // Clear file states
    memset(g_file_states, 0, sizeof(g_file_states));

    my_printf(&huart1, "Initializing data storage system...\r\n");

    // Check if SD card is ready before any file operations
    extern SD_HandleTypeDef hsd;
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd);
    if (card_state != HAL_SD_CARD_TRANSFER)
    {
        my_printf(&huart1, "Warning: SD card not ready (state=%d), skipping storage init\r\n", card_state);
        return DATA_STORAGE_NO_SD;
    }

    my_printf(&huart1, "SD card ready, mounting filesystem...\r\n");

    // Explicitly mount FATFS - MX_FATFS_Init() only links the driver, does NOT mount!
    // f_mount() must be called before any file operations
    FRESULT mount_res = f_mount(&SDFatFS, SDPath, 1);  // 1 = mount immediately
    if (mount_res != FR_OK)
    {
        my_printf(&huart1, "ERROR: f_mount failed (res=%d)\r\n", mount_res);
        return DATA_STORAGE_ERROR;
    }
    my_printf(&huart1, "Filesystem mounted successfully\r\n");

    // Create storage directories
    data_storage_status_t dir_result = create_storage_directories();
    if (dir_result != DATA_STORAGE_OK)
    {
        my_printf(&huart1, "Warning: Some directories creation failed, system may not work properly\r\n");
    }

    // Read boot count from SD card
    g_boot_count = get_boot_count_from_fatfs();
    g_boot_count++;

    // Save updated boot count
    data_storage_status_t boot_result = save_boot_count_to_fatfs(g_boot_count);
    if (boot_result != DATA_STORAGE_OK)
    {
        my_printf(&huart1, "Warning: Failed to save boot count\r\n");
    }

    my_printf(&huart1, "Data storage system initialized, boot count: %lu\r\n", g_boot_count);

    return DATA_STORAGE_OK;
}

// ============================================================================
// Public API implementation - Write functions
// ============================================================================

/**
 * @brief Write sample data
 * @param voltage Voltage value
 * @return Operation status
 */
data_storage_status_t data_storage_write_sample(float voltage)
{
    char formatted_data[128];

    data_storage_status_t result = format_sample_data(voltage, formatted_data);
    if (result != DATA_STORAGE_OK)
    {
        return result;
    }

    return write_data_to_file(STORAGE_SAMPLE, formatted_data);
}

/**
 * @brief Write over-limit data
 * @param voltage Voltage value
 * @param limit Limit threshold
 * @return Operation status
 */
data_storage_status_t data_storage_write_overlimit(float voltage, float limit)
{
    char formatted_data[128];

    data_storage_status_t result = format_overlimit_data(voltage, limit, formatted_data);
    if (result != DATA_STORAGE_OK)
    {
        return result;
    }

    return write_data_to_file(STORAGE_OVERLIMIT, formatted_data);
}

/**
 * @brief Write log data
 * @param operation Operation description
 * @return Operation status
 */
data_storage_status_t data_storage_write_log(const char *operation)
{
    char formatted_data[256];

    data_storage_status_t result = format_log_data(operation, formatted_data);
    if (result != DATA_STORAGE_OK)
    {
        return result;
    }

    return write_data_to_file(STORAGE_LOG, formatted_data);
}

/**
 * @brief Write hidden data (original + hex format)
 * @param voltage Voltage value
 * @param is_overlimit Over-limit flag
 * @return Operation status
 */
data_storage_status_t data_storage_write_hidedata(float voltage, uint8_t is_overlimit)
{
    char formatted_data[256];

    data_storage_status_t result = format_hidedata(voltage, is_overlimit, formatted_data);
    if (result != DATA_STORAGE_OK)
    {
        return result;
    }

    return write_data_to_file(STORAGE_HIDEDATA, formatted_data);
}

/**
 * @brief Test storage system
 * @return Operation status
 */
data_storage_status_t data_storage_test(void)
{
    my_printf(&huart1, "Data storage system test - placeholder\r\n");
    return DATA_STORAGE_OK;
}

// ============================================================================
// Hide mode control
// ============================================================================

/**
 * @brief Set hide mode
 * @param mode 0=normal, 1=hide
 */
void data_storage_set_hide_mode(uint8_t mode)
{
    g_hide_mode = mode;
}

/**
 * @brief Get hide mode status
 * @return 0=normal, 1=hide
 */
uint8_t data_storage_get_hide_mode(void)
{
    return g_hide_mode;
}

// ============================================================================
// Backward compatible wrapper functions
// ============================================================================

/**
 * @brief Write sample data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param voltage Voltage value
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_sample(rtc_datetime_t *dt, float voltage)
{
    (void)dt;  // Ignore dt parameter, time is fetched internally

    // In hide mode, sample data is not stored
    if (g_hide_mode)
    {
        return 0;
    }

    return (data_storage_write_sample(voltage) == DATA_STORAGE_OK) ? 0 : 1;
}

/**
 * @brief Write over-limit data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param voltage Voltage value
 * @param limit Limit threshold
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_overlimit(rtc_datetime_t *dt, float voltage, float limit)
{
    (void)dt;  // Ignore dt parameter
    return (data_storage_write_overlimit(voltage, limit) == DATA_STORAGE_OK) ? 0 : 1;
}

/**
 * @brief Write log data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param log_msg Log message string
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_log(rtc_datetime_t *dt, const char *log_msg)
{
    (void)dt;  // Ignore dt parameter
    return (data_storage_write_log(log_msg) == DATA_STORAGE_OK) ? 0 : 1;
}

/**
 * @brief Write hidden data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param voltage Voltage value
 * @param over_limit Over-limit flag
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_hidedata(rtc_datetime_t *dt, float voltage, uint8_t over_limit)
{
    (void)dt;  // Ignore dt parameter
    return (data_storage_write_hidedata(voltage, over_limit) == DATA_STORAGE_OK) ? 0 : 1;
}
