#ifndef __RTC_APP_H__
#define __RTC_APP_H__

#include "mydefine.h"
#include "rtc.h"

/* RTC Status enumeration */
typedef enum {
    RTC_STATUS_OK = 0,
    RTC_STATUS_ERROR = 1
} rtc_status_t;

/* DateTime structure for easy handling */
typedef struct {
    uint16_t year;    // Full year (e.g., 2025)
    uint8_t month;    // 1-12
    uint8_t date;     // 1-31
    uint8_t weekday;  // 1-7 (Monday=1, Sunday=7)
    uint8_t hours;    // 0-23
    uint8_t minutes;  // 0-59
    uint8_t seconds;  // 0-59
} rtc_datetime_t;

/* Function prototypes */
rtc_status_t rtc_set_datetime(const rtc_datetime_t *datetime);
rtc_status_t rtc_get_datetime(rtc_datetime_t *datetime);
rtc_status_t rtc_format_datetime_string(const rtc_datetime_t *datetime,
                                        char *buffer, uint16_t buffer_size);
rtc_status_t rtc_set_time_from_string(const char *datetime_str);
rtc_status_t rtc_get_time_string(char *buffer, uint16_t buffer_size);

/* Helper functions */
uint8_t rtc_calculate_weekday(uint16_t year, uint8_t month, uint8_t date);
uint8_t rtc_is_time_valid(void);
uint32_t datetime_to_unix(rtc_datetime_t *dt);
void RTC_Handle_Command(char *input_str);
#endif

