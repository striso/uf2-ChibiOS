#ifndef HW_CONFIG_H_
#define HW_CONFIG_H_

#include "board.h"
#include "portab.h"

#ifndef UF2_FAMILY
#define UF2_FAMILY 0x6db66082 // generic STM32H7
#endif
#define BOOTLOADER_DELAY 0
#define INTERFACE_USB 1
#define INTERFACE_USART 0

#define BOARD_USB_VBUS_SENSE_DISABLED
//# define BOARD_PIN_VBUS                 GPIO5
//# define BOARD_PORT_VBUS                GPIOC

#if !defined(BOARD_FIRST_FLASH_SECTOR_TO_ERASE)
#define BOARD_FIRST_FLASH_SECTOR_TO_ERASE 0
#endif

#ifndef USBVENDORID
#define USBVENDORID 0x26AC
#endif

#ifndef USBPRODUCTID
#define USBPRODUCTID 0x1043
#endif


#endif /* HW_CONFIG_H_ */
