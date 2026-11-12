#include "core/wall_time.h"

enum {
    FIRST_YEAR = 2000,
    FEBRUARY = 2,
    TIME_LOST = 0x80,
    SECOND_REGISTER = 0,
    MINUTE_REGISTER = 1,
    HOUR_REGISTER = 2,
    DAY_REGISTER = 3,
    MONTH_REGISTER = 5,
    YEAR_REGISTER = 6
};

static const uint8_t DAYS_IN_MONTH[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint8_t REGISTER_MASKS[WALL_TIME_REGISTER_COUNT] = {0x7F, 0x7F, 0x3F, 0x3F,
                                                                 0x00, 0x1F, 0xFF};
static const uint8_t REGISTER_LARGEST[WALL_TIME_REGISTER_COUNT] = {59, 59, 23, 31, 0, 12, 99};

bool wall_time_from_registers(const uint8_t *registers, WallTime *time)
{
    uint8_t values[WALL_TIME_REGISTER_COUNT];
    for (int index = 0; index < WALL_TIME_REGISTER_COUNT; index++) {
        uint8_t bcd = registers[index] & REGISTER_MASKS[index];
        uint8_t value = (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
        if ((bcd & 0x0F) > 9 || value > REGISTER_LARGEST[index]) {
            return false;
        }
        values[index] = value;
    }
    uint8_t month = values[MONTH_REGISTER];
    uint8_t day = values[DAY_REGISTER];
    bool leap_day = month == FEBRUARY && day == 29 && values[YEAR_REGISTER] % 4 == 0;
    if ((registers[SECOND_REGISTER] & TIME_LOST) != 0 || month == 0 || day == 0 ||
        (day > DAYS_IN_MONTH[month - 1] && !leap_day)) {
        return false;
    }
    *time = (WallTime){.year = (uint16_t)(FIRST_YEAR + values[YEAR_REGISTER]),
                       .month = month,
                       .day = day,
                       .hour = values[HOUR_REGISTER],
                       .minute = values[MINUTE_REGISTER],
                       .second = values[SECOND_REGISTER]};
    return true;
}
