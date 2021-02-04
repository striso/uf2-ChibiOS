#include "hal.h"
#include "portab.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"

/* flash parameters that we should not really know */
static struct {
	uint32_t	sector_number;
	uint32_t	size;
	uint32_t	bank;
} flash_sectors[] = {
	{0x00, 128 * 1024, FLASH_BANK_1},
	{0x01, 128 * 1024, FLASH_BANK_1},
	{0x02, 128 * 1024, FLASH_BANK_1},
	{0x03, 128 * 1024, FLASH_BANK_1},
	{0x04, 128 * 1024, FLASH_BANK_1},
	{0x05, 128 * 1024, FLASH_BANK_1},
	{0x06, 128 * 1024, FLASH_BANK_1},
	{0x07, 128 * 1024, FLASH_BANK_1},
	{0x00, 128 * 1024, FLASH_BANK_2},
	{0x01, 128 * 1024, FLASH_BANK_2},
	{0x02, 128 * 1024, FLASH_BANK_2},
	{0x03, 128 * 1024, FLASH_BANK_2},
	{0x04, 128 * 1024, FLASH_BANK_2},
	{0x05, 128 * 1024, FLASH_BANK_2},
	{0x06, 128 * 1024, FLASH_BANK_2},
	{0x07, 128 * 1024, FLASH_BANK_2},
};


uint32_t flash_func_sector_size(unsigned sector) {
	if (sector < BOARD_FLASH_SECTORS) {
		return flash_sectors[sector].size;
	}

	return 0;
}

static uint8_t erasedSectors[BOARD_FLASH_SECTORS];

static bool is_blank(uint32_t addr, uint32_t size) {
	for (unsigned i = 0; i < size; i += sizeof(uint32_t)) {
		if (*(uint32_t*)(addr + i) != 0xffffffff) {
			// DMESG("non blank: %p i=%d/%d", addr, i, size);
			return false;
		}
	}
	return true;
}

/*
 * Write flash, erase sector if necessary and not already erased.
 *
 * When failsafe=true don't check if the sector is empty to not fail on ECC errors.
 */
void flash_write(uint32_t dst, const uint8_t *src, int len, bool failsafe) {
	uint32_t addr = 0x08000000;
	uint32_t sector = 0;
	uint32_t size = 0;

	for (unsigned i = 0; i < BOARD_FLASH_SECTORS; i++) {
		size = flash_func_sector_size(i);
		if (addr + size > dst) {
			sector = i;
			break;
		}
		addr += size;
	}
	// sector = (dst - 0x08000000) / (128 * 1024);

	if (sector == 0 || sector >= BOARD_FLASH_SECTORS) {// Bootloader sector should not be erased
		return; //PANIC("invalid sector");
	}

	HAL_FLASH_Unlock();

	if (!erasedSectors[sector]) {
		if (failsafe || !is_blank(addr, size)) {
			FLASH_EraseInitTypeDef eraseInit;
			eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
			eraseInit.Banks = flash_sectors[sector].bank;
			eraseInit.Sector = flash_sectors[sector].sector_number;
			eraseInit.NbSectors = 1;
			eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

			uint32_t sectorError = 0;
			HAL_FLASHEx_Erase(&eraseInit, &sectorError);

			// if (!is_blank(addr, size) | (sectorError != 0xffffffff))
			// 	PANIC("failed to erase!");
		}
		erasedSectors[sector] = 1; // don't erase anymore - we will continue writing here!
	}

	// check if flash is really empty, otherwise ECC errors might be created
	if (is_blank(dst, len)) {
		for (int i = 0; i < len; i += 4) {
			// flash_program_word(dst + i, *(uint32_t*)(src + i));
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, dst + i, (uint32_t)src + i);
		}
	} else {
		// PANIC("flash to write is not empty");
		// TODO: better error handling
		while (true) {
			palToggleLine(PORTAB_BLINK_LED);
			chThdSleepMilliseconds(30);
		}
	}

	// TODO: implement error checking
	// ccrc = CalcCRC32((uint8_t *)(SDRAM_BANK_ADDR + 0x010), flength);

	// if (memcmp((void*)dst, src, len) != 0) {
	// 	PANIC("failed to write");
	// }

	HAL_FLASH_Lock();
}
