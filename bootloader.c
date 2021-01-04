#include "hal.h"
#include "bootloader.h"
#include "uf2cfg.h"
#include "portab.h"
#include "stm32h7xx_hal_flash.h"

typedef void (*pFunction)(void);

void jump_to_app() {
    const uint32_t *app_base = (const uint32_t *)APP_LOAD_ADDRESS;

    /*
     * We refuse to program the first word of the app until the upload is marked
     * complete by the host.  So if it's not 0xffffffff, we should try booting it.
     */
    if (app_base[0] == 0xffffffff) {
        return;
    }

    // first word is stack base - needs to be in RAM region and word-aligned
    if ((app_base[0] & 0xff000003) != 0x20000000) {
        return;
    }

    /*
     * The second word of the app is the entrypoint; it must point within the
     * flash area (or we have a bad flash).
     */
    if (app_base[1] < APP_LOAD_ADDRESS) {
        return;
    }

    if (app_base[1] >= (APP_LOAD_ADDRESS + BOARD_FLASH_SIZE)) {
        return;
    }

    /* just for paranoia's sake */
    HAL_FLASH_Lock();

  pFunction Jump_To_Application;

  /* load this address into function pointer */
  Jump_To_Application = (pFunction)app_base[1];

  /* Clear pending interrupts just to be on the safe side*/
  SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;

  /* Disable all interrupts */
  uint8_t i;
  for (i = 0; i < 8; i++)
    NVIC->ICER[i] = NVIC->IABR[i];

  /* set stack pointer as in application's vector table */
  __set_MSP((uint32_t) (app_base[0]));
  Jump_To_Application();
}

/**
 * Set boot signature and reset
 */
void reset_to_uf2_bootloader(void) {
  // Enable writing to backup domain
  // STM32F4:
  // PWR->CR |= PWR_CR_DBP;
  // STM32H7:
  PWR->CR1 |= PWR_CR1_DBP;
  // Set boot signature in RTC backup register
  RTC->BKP0R = BOOTLOADER_RTC_SIGNATURE;

  NVIC_SystemReset();
}
