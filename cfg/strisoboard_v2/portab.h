/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

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

/**
 * @file    portab.h
 * @brief   Application portability macros and structures.
 *
 * @addtogroup application_portability
 * @{
 */

#ifndef PORTAB_H
#define PORTAB_H

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

#define USBDEVICESTRING BOARD_NAME
#define USBMFGSTRING "Striso"
#define BOARD_FLASH_SECTORS 16
#define BOARD_FLASH_SIZE (2 * 1024 * 1024)

// UF2 Family ID - picked at random
#define UF2_FAMILY 0xa21e1295 // Striso board v2.0 - STM32H7

// #define PORTAB_USB1                 USBD1

// #define PORTAB_SDU1                 SDU1

#define PORTAB_BLINK_LED            LINE_LED_R
#define PORTAB_STATUS_LED           LINE_LED_UP
#define PORTAB_FLASHER_LED          LINE_LED_B
#define PORTAB_FLASHER_SKIP_LED     LINE_LED_G
#define PORTAB_BOOTLOADER_BUTTON    LINE_BUTTON_ALT
#define PORTAB_BOOTLOADER_BUTTON_PRESSED    0
#define PORTAB_FAILSAFE_BUTTON      LINE_BUTTON_PORT
#define PORTAB_FAILSAFE_BUTTON_PRESSED      0

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void portab_setup(void);
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

#endif /* PORTAB_H */

/** @} */
