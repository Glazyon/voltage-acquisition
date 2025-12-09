#include "rtc_app.h"
#include "data_storage_app.h"
#include <stdio.h>


/* External RTC handle from rtc.c */
extern RTC_HandleTypeDef hrtc;

/**
 * @brief  Calculate day of week using Zeller's congruence
 * @param  year: Full year (e.g., 2025)
 * @param  month: Month (1-12)
 * @param  date: Day of month (1-31)
 * @retval Weekday (1=Monday, 7=Sunday) matching RTC_WEEKDAY definitions
 */
uint8_t rtc_calculate_weekday(uint16_t year, uint8_t month, uint8_t date)
{
    int y = year;
    int m = month;
    int d = date;

    // Zeller's congruence for Gregorian calendar
    if (m < 3) {
        m += 12;
        y--;
    }

    int k = y % 100;        // Year of century
    int j = y / 100;        // Century

    int h = (d + (13 * (m + 1)) / 5 + k + k/4 + j/4 - 2*j) % 7;

    // Convert Zeller's result (0=Saturday) to ISO (1=Monday, 7=Sunday)
    int weekday = ((h + 5) % 7) + 1;

    return (uint8_t)weekday;
}

/**
 * @brief  Set RTC date and time
 * @param  datetime: Pointer to rtc_datetime_t structure
 * @retval RTC_STATUS_OK or RTC_STATUS_ERROR
 */
rtc_status_t rtc_set_datetime(const rtc_datetime_t *datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (datetime == NULL) return RTC_STATUS_ERROR;

    // Validate inputs
    if (datetime->year < 2000 || datetime->year > 2099) return RTC_STATUS_ERROR;
    if (datetime->month < 1 || datetime->month > 12) return RTC_STATUS_ERROR;
    if (datetime->date < 1 || datetime->date > 31) return RTC_STATUS_ERROR;
    if (datetime->hours > 23) return RTC_STATUS_ERROR;
    if (datetime->minutes > 59) return RTC_STATUS_ERROR;
    if (datetime->seconds > 59) return RTC_STATUS_ERROR;
    if (datetime->weekday < 1 || datetime->weekday > 7) return RTC_STATUS_ERROR;

    // Set time
    sTime.Hours = datetime->hours;
    sTime.Minutes = datetime->minutes;
    sTime.Seconds = datetime->seconds;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        return RTC_STATUS_ERROR;
    }

    // Set date
    sDate.Year = datetime->year - 2000;  // Convert to offset
    sDate.Month = datetime->month;
    sDate.Date = datetime->date;
    sDate.WeekDay = datetime->weekday;

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return RTC_STATUS_ERROR;
    }

    return RTC_STATUS_OK;
}

/**
 * @brief  Get current RTC date and time
 * @param  datetime: Pointer to rtc_datetime_t structure to store result
 * @retval RTC_STATUS_OK or RTC_STATUS_ERROR
 * @note   CRITICAL: Must call HAL_RTC_GetDate after HAL_RTC_GetTime
 *         to unlock shadow registers
 */
rtc_status_t rtc_get_datetime(rtc_datetime_t *datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (datetime == NULL) return RTC_STATUS_ERROR;

    // Get time first
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        return RTC_STATUS_ERROR;
    }

    // MUST get date immediately after time to unlock shadow registers
    if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        return RTC_STATUS_ERROR;
    }

    // Fill structure
    datetime->hours = sTime.Hours;
    datetime->minutes = sTime.Minutes;
    datetime->seconds = sTime.Seconds;
    datetime->year = sDate.Year + 2000;  // Convert from offset
    datetime->month = sDate.Month;
    datetime->date = sDate.Date;
    datetime->weekday = sDate.WeekDay;

    return RTC_STATUS_OK;
}

/**
 * @brief  Format datetime structure to string "YYYY-MM-DD HH:MM:SS"
 * @param  datetime: Pointer to rtc_datetime_t structure
 * @param  buffer: Pointer to output string buffer
 * @param  buffer_size: Size of output buffer (minimum 20 bytes)
 * @retval RTC_STATUS_OK or RTC_STATUS_ERROR
 */
rtc_status_t rtc_format_datetime_string(const rtc_datetime_t *datetime,
                                        char *buffer, uint16_t buffer_size)
{
    if (datetime == NULL || buffer == NULL) return RTC_STATUS_ERROR;
    if (buffer_size < 20) return RTC_STATUS_ERROR;

    // Format: "2025-01-01 12:00:30"
    snprintf(buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d",
             datetime->year,
             datetime->month,
             datetime->date,
             datetime->hours,
             datetime->minutes,
             datetime->seconds);

    return RTC_STATUS_OK;
}

/**
 * @brief  Parse and set datetime from string "YYYY-MM-DD HH:MM:SS"
 * @param  datetime_str: Input string in format "2025-01-01 12:00:30"
 * @retval RTC_STATUS_OK or RTC_STATUS_ERROR
 */
rtc_status_t rtc_set_time_from_string(const char *datetime_str)
{
    rtc_datetime_t datetime = {0};

    if (datetime_str == NULL) return RTC_STATUS_ERROR;

    // Parse string: "2025-01-01 12:00:30"
    int result = sscanf(datetime_str, "%4hu-%2hhu-%2hhu %2hhu:%2hhu:%2hhu",
                        &datetime.year,
                        &datetime.month,
                        &datetime.date,
                        &datetime.hours,
                        &datetime.minutes,
                        &datetime.seconds);

    if (result != 6) return RTC_STATUS_ERROR;

    // Calculate weekday
    datetime.weekday = rtc_calculate_weekday(datetime.year,
                                             datetime.month,
                                             datetime.date);

    return rtc_set_datetime(&datetime);
}

/**
 * @brief  Get current time as formatted string "YYYY-MM-DD HH:MM:SS"
 * @param  buffer: Pointer to output string buffer
 * @param  buffer_size: Size of output buffer (minimum 20 bytes)
 * @retval RTC_STATUS_OK or RTC_STATUS_ERROR
 */
rtc_status_t rtc_get_time_string(char *buffer, uint16_t buffer_size)
{
    rtc_datetime_t datetime;

    if (rtc_get_datetime(&datetime) != RTC_STATUS_OK) {
        return RTC_STATUS_ERROR;
    }

    return rtc_format_datetime_string(&datetime, buffer, buffer_size);
}

/**
 * @brief  Check if RTC was restored from VBAT or freshly initialized
 * @param  None
 * @retval 1 if time restored from VBAT, 0 if fresh initialization
 */
uint8_t rtc_is_time_valid(void)
{
    extern RTC_HandleTypeDef hrtc;
    uint32_t magic = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
    return (magic == 0x32F2) ? 1 : 0;
}



static uint8_t is_rtc_config_mode = 0;




void RTC_Handle_Command(char *input_str)
{

    if (is_rtc_config_mode == 0)
    {

        if (strcmp(input_str, "RTC Config") == 0)
        {
            is_rtc_config_mode = 1;
            my_printf(&huart1, "\r\n[RTC] Enter Config Mode.\r\n");
            my_printf(&huart1, "[RTC] Input Datetime (Format: YYYY-MM-DD HH:MM:SS)\r\n>> ");
            // 记录log
            rtc_datetime_t dt;
            rtc_get_datetime(&dt);
            data_storage_save_log(&dt, "rtc config");
        }

        else if (strcmp(input_str, "RTC now") == 0)
        {
            char time_buffer[30];


            if (rtc_get_time_string(time_buffer, sizeof(time_buffer)) == RTC_STATUS_OK)
            {
                my_printf(&huart1, "\r\n[RTC] Current Time: %s\r\n", time_buffer);

                // Show if time was restored from backup or reset
                if (rtc_is_time_valid()) {
                    my_printf(&huart1, "[RTC] Status: Time preserved by VBAT\r\n");
                } else {
                    my_printf(&huart1, "[RTC] Status: Fresh initialization (default time)\r\n");
                }
            }
            else
            {
                my_printf(&huart1, "\r\n[RTC] Error: Read failed.\r\n");
            }
        }

        else
        {
//            my_printf(&huart1, "Unknown command: %s\r\n", input_str);
        }
    }


    else
    {
        if (rtc_set_time_from_string(input_str) == RTC_STATUS_OK)
        {
            my_printf(&huart1, "\r\n[RTC] Success! Time updated to: %s\r\n", input_str);
            is_rtc_config_mode = 0;
            // 记录log
            rtc_datetime_t dt;
            rtc_get_datetime(&dt);
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "rtc config success to %s", input_str);
            data_storage_save_log(&dt, log_msg);
        }
        else
        {
            my_printf(&huart1, "\r\n[RTC] Error: Invalid format.\r\n");


        }
    }
}

/**
 * @brief RTC时间转Unix时间戳
 */
uint32_t datetime_to_unix(rtc_datetime_t *dt)
{
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint16_t year = dt->year;
    uint32_t days = 0;

    for (uint16_t y = 1970; y < year; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) days++;
    }

    days += days_before_month[dt->month - 1];

    if (dt->month > 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        days++;
    }

    days += dt->date - 1;

    return days * 86400UL + dt->hours * 3600UL + dt->minutes * 60UL + dt->seconds;
}




