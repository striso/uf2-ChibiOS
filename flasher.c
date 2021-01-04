#include "ch.h"
#include "hal.h"
#include <string.h>
#include "portab.h"
#include "uf2cfg.h"
#include "bootloader.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"

extern const unsigned long bindata_len;
extern const unsigned char bindata[];

/**
 *  Firmware version description on fixed flash address for bootloader
 */
__attribute__ ((section(".fwinfo"))) __attribute__((used))
const struct {
  char fwversion[512];
  uint32_t confightm[16];
} fwinfo = {
  .fwversion = "Bootloader flasher\r\n",
};

void pre_clock_init(void) {}

int main(void) {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  palClearLine(PORTAB_BLINK_LED);
  palClearLine(LINE_LED_B);

  uint32_t dst = 0x08000000;

  if (memcmp((void *)dst, bindata, bindata_len) == 0) {
    // already the same, don't flash
    palSetLine(LINE_LED_R);
    palSetLine(LINE_LED_B);
    palClearLine(LINE_LED_G);
    chThdSleepMilliseconds(500);
    reset_to_uf2_bootloader();
  }

  HAL_FLASH_Unlock();

  // erase first sector
  FLASH_EraseInitTypeDef eraseInit;
  eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
  eraseInit.Banks = FLASH_BANK_1;
  eraseInit.Sector = 0;
  eraseInit.NbSectors = 1;
  eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  uint32_t sectorError = 0;
  HAL_FLASHEx_Erase(&eraseInit, &sectorError);

  palSetLine(LINE_LED_B);

  // if (!is_blank(addr, size) | (sectorError != 0xffffffff))
  // 	PANIC("failed to erase!");

  for (uint32_t i = 0; i < bindata_len; i += 4) {
    // flash_program_word(dst + i, *(uint32_t*)(src + i));
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, dst + i,
                      (uint32_t)bindata + i);
  }

  // TODO: implement error checking
  // ccrc = CalcCRC32((uint8_t *)(SDRAM_BANK_ADDR + 0x010), flength);

  // if (memcmp((void*)dst, src, len) != 0) {
  // 	PANIC("failed to write");
  // }

  // self destruct
  uint32_t empty = 0;
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, APP_LOAD_ADDRESS, (uint32_t)&empty);
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, APP_LOAD_ADDRESS + 4, (uint32_t)&empty);

  HAL_FLASH_Lock();

  palClearLine(LINE_LED_B);
  reset_to_uf2_bootloader();
}
