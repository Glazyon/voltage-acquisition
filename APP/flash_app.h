#ifndef __FLASH_APP_H__
#define __FLASH_APP_H__

#include "mydefine.h"

// ============================================================================
// Flash Address Definitions
// ============================================================================
#define ID_ADDRESS              0x000000    // Device ID storage address
#define CONFIG_FLASH_ADDR       0x001000    // Config data storage address
#define CONFIG_MAGIC_NUMBER     0x47443332  // "GD32" magic number

// ============================================================================
// Config Storage Structure (20 bytes)
// ============================================================================
typedef struct __attribute__((packed))
{
    uint32_t magic;          // Magic number (0x47443332 = "GD32")
    float    ratio_ch0;      // [Ratio] Ch0 value
    float    limit_ch0;      // [Limit] Ch0 value
    uint8_t  is_valid;       // Valid flag
    uint8_t  reserved[3];    // Reserved for 4-byte alignment
    uint32_t crc32;          // CRC32 checksum
} flash_config_t;

// ============================================================================
// Config Operation Status
// ============================================================================
typedef enum
{
    FLASH_CONFIG_OK = 0,           // Success
    FLASH_CONFIG_READ_SD_ERR,      // SD card read error
    FLASH_CONFIG_INVALID_DATA,     // Invalid config data
    FLASH_CONFIG_WRITE_ERR,        // Flash write error
    FLASH_CONFIG_CRC_ERR           // CRC verification error
} flash_config_status_t;

// ============================================================================
// Global Variables
// ============================================================================
extern uint32_t device_id;

// ============================================================================
// Function Declarations
// ============================================================================
// Device ID operations
void flash_read_id(void);
void flash_write_id(void);
void flash_print_id(void);

// Config save operations
flash_config_status_t flash_config_save(void);
uint32_t flash_calculate_crc32(const uint8_t *data, uint32_t length);

// Config read operations
flash_config_status_t flash_config_read(flash_config_t *config);

#endif

