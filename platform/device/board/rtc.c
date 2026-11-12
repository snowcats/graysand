#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "cru.h"
#include "device/audio_math.h"
#include "gpio.h"
#include "hal/clock.h"
#include "registers.h"

#define I2C1_BASE 0x40050000u
#define I2C_CON (I2C1_BASE + 0x00u)
#define I2C_CLKDIV (I2C1_BASE + 0x04u)
#define I2C_MRXADDR (I2C1_BASE + 0x08u)
#define I2C_MTXCNT (I2C1_BASE + 0x10u)
#define I2C_MRXCNT (I2C1_BASE + 0x14u)
#define I2C_IPD (I2C1_BASE + 0x1Cu)
#define I2C_TXDATA (I2C1_BASE + 0x100u)
#define I2C_RXDATA (I2C1_BASE + 0x200u)
#define GRF_PIN_GROUPS (GRF_BASE + 0xBCu)
#define GRF_I2C1_GROUP_C 0x000C0008u
#define I2C_MRXADDR_VALID 0x01000000u

enum {
    GATE_I2C1 = 0x84,
    RESET_I2C1 = 0x1B,
    PIN_FUNCTION_I2C1 = 3,
    RESET_HOLD_US = 200,
    BUS_KHZ = 100,
    TIMEOUT_MS = 10,
    RTC_ADDRESS = 0xA2,
    FIRST_TIME_REGISTER = 0x02,
    I2C_ENABLE = 0x01,
    I2C_MODE_RECEIVE = 0x02,
    I2C_START = 0x08,
    I2C_STOP = 0x10,
    I2C_NAK_LAST = 0x20,
    I2C_ALL_PENDING = 0x7F,
    I2C_TX_DONE = 0x04,
    I2C_RX_DONE = 0x08,
    I2C_START_DONE = 0x10,
    I2C_STOP_DONE = 0x20,
    I2C_NAK = 0x40
};

static const GpioPin PINS[] = {{2, 12}, {2, 13}};

static bool wait_pending(uint32_t bits)
{
    uint32_t start = hal_clock_milliseconds();
    while ((register_read(I2C_IPD) & bits) == 0) {
        if (hal_clock_milliseconds() - start >= TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static bool start(uint32_t mode)
{
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, I2C_ENABLE | mode | I2C_START);
    bool started = wait_pending(I2C_START_DONE);
    register_write(I2C_IPD, I2C_START_DONE);
    register_write(I2C_CON, I2C_ENABLE | mode);
    return started;
}

static bool stop(void)
{
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, I2C_ENABLE | I2C_STOP);
    bool stopped = wait_pending(I2C_STOP_DONE);
    register_write(I2C_IPD, I2C_ALL_PENDING);
    register_write(I2C_CON, 0);
    return stopped;
}

static bool point_at_time(void)
{
    bool sent = start(0);
    if (sent) {
        register_write(I2C_TXDATA, (uint32_t)RTC_ADDRESS | (uint32_t)FIRST_TIME_REGISTER << 8);
        register_write(I2C_IPD, I2C_ALL_PENDING);
        register_write(I2C_MTXCNT, 2);
        sent = wait_pending(I2C_TX_DONE | I2C_NAK) && (register_read(I2C_IPD) & I2C_NAK) == 0;
    }
    return stop() && sent;
}

static bool receive(uint8_t *bytes, uint32_t count)
{
    register_write(I2C_MRXADDR, I2C_MRXADDR_VALID + RTC_ADDRESS + 1u);
    bool received = start(I2C_MODE_RECEIVE);
    if (received) {
        register_write(I2C_IPD, I2C_ALL_PENDING);
        register_write(I2C_CON, register_read(I2C_CON) | I2C_NAK_LAST);
        register_write(I2C_MRXCNT, count);
        received = wait_pending(I2C_RX_DONE);
        for (uint32_t index = 0; index < count; index++) {
            bytes[index] =
                (uint8_t)(register_read(I2C_RXDATA + (index & ~3u)) >> (8 * (index & 3u)));
        }
    }
    return stop() && received;
}

static void start_bus(void)
{
    gpio_mux(PINS[0], PIN_FUNCTION_I2C1);
    gpio_mux(PINS[1], PIN_FUNCTION_I2C1);
    register_write(GRF_PIN_GROUPS, GRF_I2C1_GROUP_C);
    cru_gate(GATE_I2C1, true);
    cru_reset(RESET_I2C1, true);
    board_delay_us(RESET_HOLD_US);
    cru_reset(RESET_I2C1, false);
    register_write(I2C_CON, 0);
    register_write(I2C_CLKDIV, i2c_clock_divider(board_pclk_hz(), BUS_KHZ));
}

bool hal_clock_wall_time(WallTime *time)
{
    uint8_t registers[WALL_TIME_REGISTER_COUNT];
    start_bus();
    bool read = point_at_time() && receive(registers, WALL_TIME_REGISTER_COUNT);
    cru_gate(GATE_I2C1, false);
    return read && wall_time_from_registers(registers, time);
}
