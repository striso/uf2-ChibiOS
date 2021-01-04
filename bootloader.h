#include "hal.h"

#define BOOTLOADER_RTC_SIGNATURE    0x71a21877
#define APP_RTC_SIGNATURE           0x24a22d12
#define POWER_DOWN_RTC_SIGNATURE    0x5019684f // Written by app fw to not re-power on.
#define HF2_RTC_SIGNATURE           0x39a63a78
#define SLEEP_RTC_ARG               0x10b37889
#define SLEEP2_RTC_ARG              0x7e3353b7

void jump_to_app(void);
void reset_to_uf2_bootloader(void);
