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
#define USBMFGSTRING "STMicroelectronics"
#define BOARD_FLASH_SECTORS 16
#define BOARD_FLASH_SIZE (2 * 1024 * 1024)

// #define PORTAB_USB1                 USBD1

// #define PORTAB_SDU1                 SDU1

#define PORTAB_BLINK_LED            LINE_LED1
#define PORTAB_STATUS_LED           LINE_LED3
#define PORTAB_FLASHER_LED          LINE_LED3
#define PORTAB_FLASHER_SKIP_LED     LINE_LED1
#define PORTAB_BOOTLOADER_BUTTON    LINE_BUTTON
#define PORTAB_BOOTLOADER_BUTTON_PRESSED    1

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
