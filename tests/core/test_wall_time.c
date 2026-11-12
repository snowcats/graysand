#include "check.h"
#include "core/wall_time.h"

static bool same(WallTime first, WallTime second)
{
    return first.year == second.year && first.month == second.month && first.day == second.day &&
           first.hour == second.hour && first.minute == second.minute &&
           first.second == second.second;
}

static void clock_registers_decode_from_bcd(void)
{
    const uint8_t registers[WALL_TIME_REGISTER_COUNT] = {0x45, 0x09, 0x21, 0x07, 0x06, 0x11, 0x26};
    WallTime time;
    CHECK(wall_time_from_registers(registers, &time));
    CHECK(same((WallTime){2026, 11, 7, 21, 9, 45}, time));
}

static void unused_register_bits_are_ignored(void)
{
    const uint8_t registers[WALL_TIME_REGISTER_COUNT] = {0x00, 0x80, 0xC0, 0xC1, 0xF8, 0xE1, 0x30};
    WallTime time;
    CHECK(wall_time_from_registers(registers, &time));
    CHECK(same((WallTime){2030, 1, 1, 0, 0, 0}, time));
}

static void a_lost_or_impossible_time_is_refused(void)
{
    const uint8_t lost[WALL_TIME_REGISTER_COUNT] = {0x80, 0x00, 0x12, 0x03, 0x00, 0x01, 0x26};
    const uint8_t no_month[WALL_TIME_REGISTER_COUNT] = {0x00, 0x00, 0x12, 0x03, 0x00, 0x00, 0x26};
    const uint8_t bad_digit[WALL_TIME_REGISTER_COUNT] = {0x0A, 0x00, 0x12, 0x03, 0x00, 0x01, 0x26};
    const uint8_t february_30[WALL_TIME_REGISTER_COUNT] = {0x00, 0x00, 0x12, 0x30,
                                                           0x00, 0x02, 0x24};
    WallTime time = {0};
    CHECK(!wall_time_from_registers(lost, &time));
    CHECK(!wall_time_from_registers(no_month, &time));
    CHECK(!wall_time_from_registers(bad_digit, &time));
    CHECK(!wall_time_from_registers(february_30, &time));
    const uint8_t leap_day[WALL_TIME_REGISTER_COUNT] = {0x00, 0x00, 0x12, 0x29, 0x00, 0x02, 0x24};
    const uint8_t not_leap[WALL_TIME_REGISTER_COUNT] = {0x00, 0x00, 0x12, 0x29, 0x00, 0x02, 0x25};
    const uint8_t late_hour[WALL_TIME_REGISTER_COUNT] = {0x00, 0x00, 0x24, 0x01, 0x00, 0x01, 0x26};
    CHECK(!wall_time_from_registers(not_leap, &time));
    CHECK(!wall_time_from_registers(late_hour, &time));
    CHECK_EQUAL(0, time.year);
    CHECK(wall_time_from_registers(leap_day, &time));
    CHECK(same((WallTime){2024, 2, 29, 12, 0, 0}, time));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(clock_registers_decode_from_bcd),
        TEST_CASE(unused_register_bits_are_ignored),
        TEST_CASE(a_lost_or_impossible_time_is_refused),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
