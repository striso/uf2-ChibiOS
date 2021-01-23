#ifndef UF2_VERSION
// Fallback version number if not provided by git
#define UF2_VERSION "1.0"
#endif
#ifndef BOARD_ID
#define BOARD_ID "Striso board v2.0"
#endif
#define INDEX_URL "http://www.striso.org"
#define VOLUME_LABEL "StrisoFW"
// Size of the USB drive
#define UF2_NUM_BLOCKS (8000000/512)
// Where the UF2 files are allowed to write data
#define USER_FLASH_START 0x08020000
#define USER_FLASH_END (0x08000000+BOARD_FLASH_SIZE)
// Address where the executable code is located
#define APP_LOAD_ADDRESS 0x08041000
// Address where firmware info string is put
#define FWVERSIONFILE 0x08040000
// Use config.uf2 and config.htm files
#define USE_CONFIGFILE
#define CFGUF2_ADDRESS 0x08020000
// Address where pointers to the config.htm segments are located
#define CONFIGHTM_FILE 0x08040200
#define CONFIGHTM_SEGMENTS 8
// Address after which the flash is protected for writing only device specific data (every 256 bytes should start with the UID)
#define DEVSPEC_FLASH_START 0x081e0000
