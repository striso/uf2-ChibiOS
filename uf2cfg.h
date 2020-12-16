#ifndef UF2_VERSION
#define UF2_VERSION "0.9"
#endif
#ifndef BOARD_ID
#define BOARD_ID "Striso board v2.0"
#endif
#define INDEX_URL "http://www.striso.org"
#define UF2_NUM_BLOCKS 8000
#define VOLUME_LABEL "StrisoFW"
// where the UF2 files are allowed to write data - we allow MBR, since it seems part of the softdevice .hex file
// #define USER_FLASH_START (uint32_t)(APP_LOAD_ADDRESS)
#define APP_LOAD_ADDRESS 0x08041000
#define USER_FLASH_START 0x08020000
#define USER_FLASH_END (0x08000000+BOARD_FLASH_SIZE)
// Address where firmware info string is put (adjust USER_FLASH_START so it's writable)
#define FWVERSIONFILE 0x8040000
#define CONFIGHTM_FILE 0x8040200
#define CONFIGHTM_SEGMENTS 8
