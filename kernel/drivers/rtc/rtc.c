/* RTC (Real-Time Clock) Driver
 * Reads time from CMOS RTC chip
 */

#include <drivers/rtc.h>
#include <kernel/kernel.h>

#define CMOS_ADDRESS    0x70
#define CMOS_DATA       0x71

#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_WEEKDAY     0x06
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_CENTURY     0x32
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B

static int8_t timezone_offset = -5;

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int rtc_update_in_progress(void)
{
    outb(CMOS_ADDRESS, RTC_STATUS_A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_init(void)
{
    /* RTC doesn't need special init for reading 
     * Just ensure we can read from it */
}

void rtc_get_time(rtc_time_t *time)
{
    uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year;
    
    while (rtc_update_in_progress());
    
    time->second = cmos_read(RTC_SECONDS);
    time->minute = cmos_read(RTC_MINUTES);
    time->hour = cmos_read(RTC_HOURS);
    time->day = cmos_read(RTC_DAY);
    time->month = cmos_read(RTC_MONTH);
    time->year = cmos_read(RTC_YEAR);
    time->weekday = cmos_read(RTC_WEEKDAY);
    
    do {
        last_second = time->second;
        last_minute = time->minute;
        last_hour = time->hour;
        last_day = time->day;
        last_month = time->month;
        last_year = time->year;
        
        while (rtc_update_in_progress());
        
        time->second = cmos_read(RTC_SECONDS);
        time->minute = cmos_read(RTC_MINUTES);
        time->hour = cmos_read(RTC_HOURS);
        time->day = cmos_read(RTC_DAY);
        time->month = cmos_read(RTC_MONTH);
        time->year = cmos_read(RTC_YEAR);
        time->weekday = cmos_read(RTC_WEEKDAY);
    } while (last_second != time->second || last_minute != time->minute ||
             last_hour != time->hour || last_day != time->day ||
             last_month != time->month || last_year != time->year);
    
    uint8_t status_b = cmos_read(RTC_STATUS_B);
    
    if (!(status_b & 0x04)) {
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour = bcd_to_bin(time->hour & 0x7F) | (time->hour & 0x80);
        time->day = bcd_to_bin(time->day);
        time->month = bcd_to_bin(time->month);
        time->year = bcd_to_bin(time->year);
        time->weekday = bcd_to_bin(time->weekday);
    }
    
    if (!(status_b & 0x02) && (time->hour & 0x80)) {
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }
    
    time->year += 2000;
}

void rtc_set_timezone(int8_t offset_hours)
{
    if (offset_hours >= -12 && offset_hours <= 14) {
        timezone_offset = offset_hours;
    }
}

int8_t rtc_get_timezone(void)
{
    return timezone_offset;
}

void rtc_get_local_time(rtc_time_t *time)
{
    rtc_get_time(time);
    
    int16_t hour = time->hour + timezone_offset;
    
    if (hour < 0) {
        hour += 24;
    } else if (hour >= 24) {
        hour -= 24;
    }
    
    time->hour = (uint8_t)hour;
}

void rtc_format_time(rtc_time_t *time, char *buf)
{
    buf[0] = '0' + (time->hour / 10);
    buf[1] = '0' + (time->hour % 10);
    buf[2] = ':';
    buf[3] = '0' + (time->minute / 10);
    buf[4] = '0' + (time->minute % 10);
    buf[5] = ':';
    buf[6] = '0' + (time->second / 10);
    buf[7] = '0' + (time->second % 10);
    buf[8] = '\0';
}

void rtc_format_date(rtc_time_t *time, char *buf)
{
    buf[0] = '0' + (time->year / 1000);
    buf[1] = '0' + ((time->year / 100) % 10);
    buf[2] = '0' + ((time->year / 10) % 10);
    buf[3] = '0' + (time->year % 10);
    buf[4] = '-';
    buf[5] = '0' + (time->month / 10);
    buf[6] = '0' + (time->month % 10);
    buf[7] = '-';
    buf[8] = '0' + (time->day / 10);
    buf[9] = '0' + (time->day % 10);
    buf[10] = '\0';
}
