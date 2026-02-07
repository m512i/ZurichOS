#ifndef _DRIVERS_RTC_H
#define _DRIVERS_RTC_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;
} rtc_time_t;

void rtc_init(void);
void rtc_get_time(rtc_time_t *time);
void rtc_set_timezone(int8_t offset_hours);
int8_t rtc_get_timezone(void);
void rtc_get_local_time(rtc_time_t *time);
void rtc_format_time(rtc_time_t *time, char *buf);
void rtc_format_date(rtc_time_t *time, char *buf);

#endif
