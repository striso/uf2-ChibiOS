# UF2-ChibiOS Bootloader for STM32H7

This bootloader implements firmware updating via USB mass storage using the [UF2 format](https://github.com/Microsoft/uf2). It uses [ChibiOS](http://www.chibios.com/) HAL and OS. It currently supports the STM32H7 microcontroller on the Striso board and the ST-Nucleo-H743ZI, and should be easy to port to other boards and microcontrollers.

This bootloader is developed for the [Striso board MPE MIDI controller](https://www.striso.org/), and borrows parts from the [STM32F UF2 bootloader](https://github.com/mmoskal/uf2-stm32f).

## Features

- Easy firmware flashing without extra software or hardware, using any device that supports USB flash drives.
- Enter bootloader by holding button on power-up, or enter through soft boot from main firmware.
- INFO_UF2.TXT file containing the current bootloader git revision.
- INFO_FW.TXT file containing the current firmware version (should be at the start of the firmware binary, see [Striso control firmware repository](https://github.com/striso/striso-control-firmware) for details)
- CONFIG.UF2 and CONFIG.HTM for firmware settings (loaded from firmware).
- Protected flash sector for storing device specific information.
- Failsafe mode for when there are flash ECC errors, started by pressing an extra button on boot.
- CF2 and WebUSB are currently not implemented.

## Build instructions

Init submodules.

To build run ``make -f make/BOARDNAME.make`` in this folder.

The binaries will be in `build/BOARDNAME`.
The following files will be built:
* `bootloader.elf` - for use with SWD/JTAG adapters
* `bootloader.bin` - for direct onboard upgrading
* `flasher.uf2` - if you already have a UF2 bootloader, you can just drop this on board and it will update the bootloader

## Adding boards

It should be relatively easy to port this bootloader to other boards and microcontrollers supported by ChibiOS. Note that the board.c file needs to have a call to pre_clock_init() for the bootloader jump. Also note that for the bootloader to work there need to be multiple flash sectors available, so the STM32H7 value line with only 1 sector of 128kB is not supported.
