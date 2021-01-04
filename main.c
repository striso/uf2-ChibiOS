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
 * Application entry point.
 */
int main(void) {
  bool try_boot = true;

  /* Check bootloader button */
  if (check_bootloader_button()) {
    try_boot = false;
  }

  // TODO: Check RTC ram for software boot into bootloader

  if (try_boot) {
    jump_to_app();
  }

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
  msdStart(&USBMSD1, &USBD1, (BaseBlockDevice *)&ghostdisk, blkbuf, NULL, NULL);

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
