/*
    ChibiOS/HAL - Copyright (C) 2016 Uladzimir Pylinsky aka barthess

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "usbcfg.h"
#include "hal_usb_msd.h"

#include "portab.h"

#include "uf2.h"
#include "ghostdisk.h"
#include "ghostfat.h"

#include "bootloader.h"

#define GHOSTDISK_BLOCK_SIZE    512U
#define GHOSTDISK_BLOCK_CNT     UF2_NUM_BLOCKS

/**
 * SCSI inquiry response structure.
 */
static const scsi_inquiry_response_t scsi_inquiry_response = {
    0x00,           /* direct access block device     */
    0x80,           /* removable                      */
    0x04,           /* SPC-2                          */
    0x02,           /* response data format           */
    0x20,           /* response has 0x20 + 4 bytes    */
    0x00,
    0x00,
    0x00,
    "Striso",       /* 8 char vendorID */
    "UF2 Bootloader",   /* 16 char productID */
    {'v',CH_KERNEL_MAJOR+'0','.',CH_KERNEL_MINOR+'0'} /* 4 char productRev */
};

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    systime_t time;

    time = USBD1.state == USB_ACTIVE ? 100 : 500;
    palSetLine(PORTAB_BLINK_LED);
    chThdSleepMilliseconds(time);
    palClearLine(PORTAB_BLINK_LED);
    chThdSleepMilliseconds(time);
  }
}

GhostDisk ghostdisk;
static uint8_t blkbuf[GHOSTDISK_BLOCK_SIZE];

BaseSequentialStream *GlobalDebugChannel;

static const SerialConfig sercfg = {
    115200,
    0,
    0,
    0
};

/* Check bootloader button */
bool check_bootloader_button(void) {
  int samples, vote = 0;
  for (samples = 0; samples < 200; samples++) {
    if (palReadLine(PORTAB_BOOTLOADER_BUTTON) == PORTAB_BOOTLOADER_BUTTON_PRESSED) {
      vote++;
    }
  }
  /* reject a little noise */
  return (vote * 100) > (samples * 90);
}

/*
 * Bootloader function, should be called from __early_init() in board.c,
 * just after stm32_gpio_init() and before stm32_clock_init()
 */
void pre_clock_init(void) {
  bool try_boot = true;

  /* Check bootloader button */
  if (check_bootloader_button()) {
    try_boot = false;
  }

  /* Check backup register for soft boot into app */
  if (RTC->BKP0R == APP_RTC_SIGNATURE) {
    // Enable writing to backup domain
    PWR->CR1 |= PWR_CR1_DBP;
    // reset signature
    RTC->BKP0R = 0;
    try_boot = true;
  }

  /* Check backup register for soft boot into bootloader */
  if (RTC->BKP0R == BOOTLOADER_RTC_SIGNATURE) {
    // Enable writing to backup domain
    PWR->CR1 |= PWR_CR1_DBP;
    // reset signature
    RTC->BKP0R = 0;
    try_boot = false;
  }

  if (try_boot) {
    jump_to_app();
  }
}

/*
 * Application entry point.
 */
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

  sdStart(&SD3, &sercfg);
  GlobalDebugChannel = (BaseSequentialStream *)&SD3;

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(&USBD1);
  chThdSleepMilliseconds(1500);
  usbStart(&USBD1, &usbcfg);

  /*
   * start Ghost Disk
   */
  ghostdiskObjectInit(&ghostdisk);
  ghostdiskStart(&ghostdisk, GHOSTDISK_BLOCK_SIZE,
                 GHOSTDISK_BLOCK_CNT, false);

  /*
   * start mass storage
   */
  msdObjectInit(&USBMSD1);
  msdStart(&USBMSD1, &USBD1, (BaseBlockDevice *)&ghostdisk, blkbuf, &scsi_inquiry_response, NULL);

  /*
   *
   */
  usbConnectBus(&USBD1);

  /*
   * Starting threads.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (true) {
    chThdSleepMilliseconds(1);
    ghostfat_1ms();
  }

  msdStop(&USBMSD1);
}
