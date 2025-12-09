// data_storage_app.h
// Data storage module header file
// Supports TF card file storage and data format management
// Copyright (c) 2024. All rights reserved.

#ifndef __DATA_STORAGE_APP_H__
#define __DATA_STORAGE_APP_H__

#include "mydefine.h"

// ============================================================================
// Storage type enumeration
// ============================================================================
typedef enum {
    STORAGE_SAMPLE = 0,    // Sample data
    STORAGE_OVERLIMIT,     // Over-limit data
    STORAGE_LOG,           // Log data
    STORAGE_HIDEDATA,      // Hidden data
    STORAGE_TYPE_COUNT     // Total count
} storage_type_t;

// ============================================================================
// Return status enumeration
// ============================================================================
typedef enum {
    DATA_STORAGE_OK = 0,       // Success
    DATA_STORAGE_ERROR,        // General error
    DATA_STORAGE_NO_SD,        // SD card not ready
    DATA_STORAGE_INVALID       // Invalid parameter
} data_storage_status_t;

// ============================================================================
// File state structure
// ============================================================================
typedef struct {
    char current_filename[64];  // Current filename
    uint8_t data_count;         // Data count in current file
    uint8_t file_exists;        // File exists flag
} file_state_t;

// ============================================================================
// Public API function declarations - New API
// ============================================================================

/**
 * @brief Initialize storage system
 * @return Operation status
 */
data_storage_status_t data_storage_init(void);

/**
 * @brief Write sample data
 * @param voltage Voltage value
 * @return Operation status
 */
data_storage_status_t data_storage_write_sample(float voltage);

/**
 * @brief Write over-limit data
 * @param voltage Voltage value
 * @param limit Limit threshold
 * @return Operation status
 */
data_storage_status_t data_storage_write_overlimit(float voltage, float limit);

/**
 * @brief Write log data
 * @param operation Operation description string
 * @return Operation status
 */
data_storage_status_t data_storage_write_log(const char *operation);

/**
 * @brief Write hidden data (original + hex format)
 * @param voltage Voltage value
 * @param is_overlimit Over-limit flag (0=normal, 1=over-limit)
 * @return Operation status
 */
data_storage_status_t data_storage_write_hidedata(float voltage, uint8_t is_overlimit);

/**
 * @brief Test storage system
 * @return Operation status
 */
data_storage_status_t data_storage_test(void);

// ============================================================================
// Utility function declarations
// ============================================================================

/**
 * @brief Generate datetime string (YYYYMMDDHHmmss format)
 * @param datetime_str Output buffer (at least 15 bytes)
 * @return Operation status
 */
data_storage_status_t generate_datetime_string(char *datetime_str);

/**
 * @brief Generate filename for specified storage type
 * @param type Storage type
 * @param filename Output buffer (at least 64 bytes)
 * @return Operation status
 */
data_storage_status_t generate_filename(storage_type_t type, char *filename);

// ============================================================================
// Hide mode control functions
// ============================================================================

/**
 * @brief Set hide mode
 * @param mode 0=normal, 1=hide
 */
void data_storage_set_hide_mode(uint8_t mode);

/**
 * @brief Get hide mode status
 * @return 0=normal, 1=hide
 */
uint8_t data_storage_get_hide_mode(void);

// ============================================================================
// Backward compatible wrapper functions (for existing code)
// ============================================================================

/**
 * @brief Write sample data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored, time is fetched internally)
 * @param voltage Voltage value
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_sample(rtc_datetime_t *dt, float voltage);

/**
 * @brief Write over-limit data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param voltage Voltage value
 * @param limit Limit threshold
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_overlimit(rtc_datetime_t *dt, float voltage, float limit);

/**
 * @brief Write log data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param log_msg Log message string
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_log(rtc_datetime_t *dt, const char *log_msg);

/**
 * @brief Write hidden data (backward compatible)
 * @param dt RTC datetime structure pointer (ignored)
 * @param voltage Voltage value
 * @param over_limit Over-limit flag
 * @return 0=success, 1=failure
 */
uint8_t data_storage_save_hidedata(rtc_datetime_t *dt, float voltage, uint8_t over_limit);

#endif /* __DATA_STORAGE_APP_H__ */
