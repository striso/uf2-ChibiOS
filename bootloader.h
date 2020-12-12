#include "hal.h"

#define BOOT_RTC_SIGNATURE          0x71a21877
#define APP_RTC_SIGNATURE           0x24a22d12
#define POWER_DOWN_RTC_SIGNATURE    0x5019684f // Written by app fw to not re-power on.
#define HF2_RTC_SIGNATURE           0x39a63a78
#define SLEEP_RTC_ARG               0x10b37889
#define SLEEP2_RTC_ARG              0x7e3353b7

#define BOOT_RTC_REG                MMIO32(RTC_BASE + 0x50)
#define ARG_RTC_REG                MMIO32(RTC_BASE + 0x54)


void jump_to_app(void);

// static uint32_t board_get_rtc_signature(uint32_t *arg);
// void board_set_rtc_signature(uint32_t sig, uint32_t arg);