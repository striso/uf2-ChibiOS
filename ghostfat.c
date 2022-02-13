/*
The MIT License (MIT)

Copyright (c) 2018 Microsoft Corp.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "hal.h"
#include "portab.h"
#include "uf2.h"
#include "flash.h"
#include <string.h>

typedef struct {
    uint8_t JumpInstruction[3];
    uint8_t OEMInfo[8];
    uint16_t SectorSize;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FATCopies;
    uint16_t RootDirectoryEntries;
    uint16_t TotalSectors16;
    uint8_t MediaDescriptor;
    uint16_t SectorsPerFAT;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    uint8_t PhysicalDriveNum;
    uint8_t Reserved;
    uint8_t ExtendedBootSig;
    uint32_t VolumeSerialNumber;
    uint8_t VolumeLabel[11];
    uint8_t FilesystemIdentifier[8];
} __attribute__((packed)) FAT_BootBlock;

typedef struct {
    char name[8];
    char ext[3];
    uint8_t attrs;
    uint8_t reserved;
    uint8_t createTimeFine;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t highStartCluster;
    uint16_t updateTime;
    uint16_t updateDate;
    uint16_t startCluster;
    uint32_t size;
} __attribute__((packed)) DirEntry;

/**
 * Length of text file string, terminated by \0 or invalid utf-8
 * and max 512 bytes long to fit in a single block.
 */
size_t fileLength(const char *s) {
    const char *s0 = s;
    while (*s++ && *s < 0xf8 && s <= s0 + 512)
        ;
    return (s - s0) - 1;
}

typedef struct {
    uint32_t addr;
    uint32_t length;
} FileSegment;
/**
 * Size of segmented file
 */
size_t segmentedFileLength(uint32_t addr, int n) {
    FileSegment *f = (FileSegment*)addr;
    size_t size = 0;
    for (int i=0; i<n; i++) {
        if (f[i].addr < USER_FLASH_START || (f[i].addr + f[i].length) >= USER_FLASH_END) {
            break;
        } else {
            size += f[i].length;
        }
    }
    return size;
}

/**
 * Get segmented file sector
 */
void segmentedFileGetSector(uint32_t addr, int n, int sectorIdx, uint8_t *data) {
    FileSegment *f = (FileSegment*)addr;
    unsigned int pfile = 0, psector = 0;
    unsigned int begin = sectorIdx * 512;
    unsigned int end = (sectorIdx + 1) * 512;
    for (int i=0; i<n; i++) {
        if (f[i].addr < USER_FLASH_START || (f[i].addr + f[i].length) >= USER_FLASH_END) {
            break;
        } else {
            if (pfile < end && (pfile + f[i].length) > begin) {
                int offset = pfile >= begin ? 0 : begin - pfile;
                int length = f[i].length - offset;
                if (psector + length > 512) {length = 512 - psector;}
                memcpy(data + psector, (void*)(f[i].addr + offset), length);
                psector += length;
            }
            pfile += f[i].length;
        }
    }
}

#define DMESG(...) ((void)0)
//#define DBG NOOP
#define DBG DMESG

#define VALID_FLASH_ADDR(addr, sz) (USER_FLASH_START <= (addr) && (addr) + (sz) <= USER_FLASH_END)

struct TextFile {
    const char name[11];
    const char *content;
};

#define NUM_FAT_BLOCKS UF2_NUM_BLOCKS

#define STR0(x) #x
#define STR(x) STR0(x)
const char infoUf2File[] = //
    "UF2-ChibiOS Bootloader " UF2_VERSION "\r\n"
    "Model: " USBMFGSTRING " / " USBDEVICESTRING "\r\n"
    "Board-ID: " BOARD_ID "\r\n";

const char indexFile[] = //
    "<!doctype html>\n"
    "<html>"
    "<body>"
    "<script>\n"
    "location.replace(\"" INDEX_URL "\");\n"
    "</script>"
    "</body>"
    "</html>\n";

// File list
static const struct TextFile info[] = {
    // Simple text files, max 512 bytes per file
    {.name = "INFO_UF2TXT", .content = infoUf2File},
    {.name = "INDEX   HTM", .content = indexFile},
#ifdef FWVERSIONFILE
    {.name = "INFO_FW TXT", .content = (char*)FWVERSIONFILE},
#endif
    // Custom handled files
    {.name = "CURRENT UF2"},
#ifdef USE_CONFIGFILE
    {.name = "CONFIG  UF2"},
    {.name = "CONFIG  HTM"},
#endif
};
#define NUM_INFO (int)(sizeof(info) / sizeof(info[0]))
#ifdef FWVERSIONFILE
#define START_CUSTOM_FILES 3
#else
#define START_CUSTOM_FILES 2
#endif

#define UF2_INDEX START_CUSTOM_FILES
#define UF2_SIZE (BOARD_FLASH_SIZE * 2)
#define UF2_SECTORS (UF2_SIZE / 512)
#define UF2_FIRST_SECTOR (START_CUSTOM_FILES)
#define UF2_LAST_SECTOR (uint32_t)(UF2_FIRST_SECTOR + UF2_SECTORS - 1)

#ifdef USE_CONFIGFILE
#define CFGUF2_INDEX (START_CUSTOM_FILES + 1)
#define CFGUF2_SIZE (128 * 1024 * 2)
#define CFGUF2_SECTORS (CFGUF2_SIZE / 512)
#define CFGUF2_FIRST_SECTOR (UF2_LAST_SECTOR + 1)
#define CFGUF2_LAST_SECTOR (CFGUF2_FIRST_SECTOR + CFGUF2_SECTORS - 1)

size_t cfghtm_size;
#define CFGHTM_INDEX (START_CUSTOM_FILES + 2)
#define CFGHTM_SIZE cfghtm_size
#define CFGHTM_SECTORS ((CFGHTM_SIZE + 511) / 512)
#define CFGHTM_FIRST_SECTOR (CFGUF2_LAST_SECTOR + 1)
#define CFGHTM_LAST_SECTOR (CFGHTM_FIRST_SECTOR + CFGHTM_SECTORS - 1)
#endif

#define RESERVED_SECTORS 1
#define ROOT_DIR_SECTORS 4
#define CLUSTER_OFFSET 2
#define SECTORS_PER_FAT ((NUM_FAT_BLOCKS * 2 + 511) / 512)

#define START_FAT0 RESERVED_SECTORS
#define START_FAT1 (START_FAT0 + SECTORS_PER_FAT)
#define START_ROOTDIR (START_FAT1 + SECTORS_PER_FAT)
#define START_CLUSTERS (START_ROOTDIR + ROOT_DIR_SECTORS)

static const FAT_BootBlock BootBlock = {
    .JumpInstruction = {0xeb, 0x3c, 0x90},
    .OEMInfo = "UF2 UF2 ",
    .SectorSize = 512,
    .SectorsPerCluster = 1,
    .ReservedSectors = RESERVED_SECTORS,
    .FATCopies = 2,
    .RootDirectoryEntries = (ROOT_DIR_SECTORS * 512 / 32),
    .TotalSectors16 = NUM_FAT_BLOCKS - 2,
    .MediaDescriptor = 0xF8,
    .SectorsPerFAT = SECTORS_PER_FAT,
    .SectorsPerTrack = 1,
    .Heads = 1,
    .ExtendedBootSig = 0x29,
    .VolumeSerialNumber = 0x00420042,
    .VolumeLabel = VOLUME_LABEL,
    .FilesystemIdentifier = "FAT16   ",
};

static uint32_t resetTime;
static uint32_t ms;
static bool failsafe_mode = false;

static void uf2_timer_start(int delay) {
    resetTime = ms + delay;
}

// called roughly every 1ms
void ghostfat_1ms(void) {
    ms++;

    if (resetTime && ms >= resetTime) {
        NVIC_SystemReset();
        while (1)
            ;
    }
}

static void padded_memcpy(char *dst, const char *src, int len) {
    for (int i = 0; i < len; ++i) {
        if (*src)
            *dst = *src++;
        else
            *dst = ' ';
        dst++;
    }
}

int read_block(uint32_t block_no, uint8_t *data) {
    memset(data, 0, 512);
    uint32_t sectionIdx = block_no;

    if (block_no == 0) {
        // Send boot block
        memcpy(data, &BootBlock, sizeof(BootBlock));
        data[510] = 0x55;
        data[511] = 0xaa;
        // logval("data[0]", data[0]);
    } else if (block_no < START_ROOTDIR) {
        // Send FAT0 or FAT1 (copy)
        sectionIdx -= START_FAT0;
        // logval("sidx", sectionIdx);
        if (sectionIdx >= SECTORS_PER_FAT)
            sectionIdx -= SECTORS_PER_FAT;
        if (sectionIdx == 0) {
            data[0] = 0xf0;
            for (int i = 1; i < NUM_INFO * 2 + 4; ++i) {
                data[i] = 0xff;
            }
        }
        for (int i = 0; i < 256; ++i) {
            uint32_t v = sectionIdx * 256 + i;
            uint32_t sector = v - CLUSTER_OFFSET;
            if (UF2_FIRST_SECTOR <= sector && sector <= UF2_LAST_SECTOR) {
                ((uint16_t *)(void *)data)[i] = (sector == UF2_LAST_SECTOR) ? 0xffff : v + 1;
            }
#ifdef USE_CONFIGFILE
            else if (CFGUF2_FIRST_SECTOR <= sector && sector <= CFGUF2_LAST_SECTOR) {
                ((uint16_t *)(void *)data)[i] = (sector == CFGUF2_LAST_SECTOR) ? 0xffff : v + 1;
            }
            else if (CFGHTM_FIRST_SECTOR <= sector && sector <= CFGHTM_LAST_SECTOR) {
                ((uint16_t *)(void *)data)[i] = (sector == CFGHTM_LAST_SECTOR) ? 0xffff : v + 1;
            }
#endif
        }
    } else if (block_no < START_CLUSTERS) {
        // Send root dir entry
        sectionIdx -= START_ROOTDIR;
        if (sectionIdx == 0) {
            DirEntry *d = (void *)data;
            padded_memcpy(d->name, (const char *)BootBlock.VolumeLabel, 11);
            d->attrs = 0x28;
            for (int i = 0; i < NUM_INFO; ++i) {
                d++;
                if (failsafe_mode && i >= 1) {
                    break;
                }
                const struct TextFile *inf = &info[i];
                if (i < START_CUSTOM_FILES) {
                    d->size = inf->content ? fileLength(inf->content) : 0;
                    d->startCluster = i + CLUSTER_OFFSET;
                }
                else if (i == UF2_INDEX) {
                    d->size = UF2_SIZE;
                    d->startCluster = UF2_FIRST_SECTOR + CLUSTER_OFFSET;
                }
#ifdef USE_CONFIGFILE
                else if (i == CFGUF2_INDEX) {
                    d->size = CFGUF2_SIZE;
                    d->startCluster = CFGUF2_FIRST_SECTOR + CLUSTER_OFFSET;
                }
                else if (i == CFGHTM_INDEX) {
                    d->size = CFGHTM_SIZE;
                    d->startCluster = CFGHTM_FIRST_SECTOR + CLUSTER_OFFSET;
                }
#endif
                padded_memcpy(d->name, inf->name, 11);
            }
        }
    } else {
        // Send file contents
        sectionIdx -= START_CLUSTERS;
        if (failsafe_mode && sectionIdx >= 1) {
            return 0;
        }
        if (sectionIdx < START_CUSTOM_FILES) {
            // Send text file content from info struct (max 1 sector per file)
            memcpy(data, info[sectionIdx].content, fileLength(info[sectionIdx].content));
        } else {
            // Custom file handling
            if (sectionIdx <= UF2_LAST_SECTOR) {
                // Send CURRENT.UF2 file
                uint32_t blockNo = sectionIdx - UF2_FIRST_SECTOR;
                uint32_t addr = blockNo * 256 + 0x08000000;
                UF2_Block *bl = (void *)data;
                bl->magicStart0 = UF2_MAGIC_START0;
                bl->magicStart1 = UF2_MAGIC_START1;
                bl->flags = UF2_FLAG_FAMILYID_PRESENT;
                bl->targetAddr = addr;
                bl->payloadSize = 256;
                bl->blockNo = blockNo;
                bl->numBlocks = BOARD_FLASH_SIZE / 256;
                bl->familyID = UF2_FAMILY;
                bl->magicEnd = UF2_MAGIC_END;

                memcpy(bl->data, (void *)addr, bl->payloadSize);
            }
#ifdef USE_CONFIGFILE
            else if (sectionIdx <= CFGUF2_LAST_SECTOR) {
                // Send CONFIG.UF2
                uint32_t blockNo = sectionIdx - CFGUF2_FIRST_SECTOR;
                uint32_t addr = blockNo * 256 + CFGUF2_ADDRESS;
                UF2_Block *bl = (void *)data;
                bl->magicStart0 = UF2_MAGIC_START0;
                bl->magicStart1 = UF2_MAGIC_START1;
                bl->flags = UF2_FLAG_FAMILYID_PRESENT;
                bl->targetAddr = addr;
                bl->payloadSize = 256;
                bl->blockNo = blockNo;
                bl->numBlocks = CFGUF2_SIZE / 256;
                bl->familyID = UF2_FAMILY;
                bl->magicEnd = UF2_MAGIC_END;

                memcpy(bl->data, (void *)addr, bl->payloadSize);
            }
            else if (sectionIdx <= CFGHTM_LAST_SECTOR) {
                // Send CONFIG.HTM
                uint32_t blockNo = sectionIdx - CFGHTM_FIRST_SECTOR;
                segmentedFileGetSector(CONFIGHTM_FILE, CONFIGHTM_SEGMENTS, blockNo, data);
            }
#endif
        }
    }

    return 0;
}

WriteState wrState; // zero initialized

int write_block(uint32_t block_no, const uint8_t *data) {
    (void)block_no;
    const UF2_Block *bl = (const void *)data;

    if (!is_uf2_block(bl) || !UF2_IS_MY_FAMILY(bl) ||
        bl->numBlocks == 0 || bl->numBlocks >= MAX_BLOCKS) {
        return 0;
    }

    if (wrState.numBlocks == 0) {
        wrState.numBlocks = bl->numBlocks;
    }

    if (wrState.numWritten >= wrState.numBlocks) {
        // writing finished, don't attempt to write more
        return 0;
    }

    if (bl->numBlocks != wrState.numBlocks ||
        bl->blockNo >= bl->numBlocks) {
        return 0; // TODO: error handling?
    }

    bool UID_check = true;
#ifdef DEVSPEC_FLASH_START
    // last sector is used to store device specific information, and should only be written if UID matches
    if (bl->targetAddr >= DEVSPEC_FLASH_START) {
        UID_check = ((uint32_t*)bl->data)[0] == ((uint32_t*)UID_BASE)[0] &&
                    ((uint32_t*)bl->data)[1] == ((uint32_t*)UID_BASE)[1] &&
                    ((uint32_t*)bl->data)[2] == ((uint32_t*)UID_BASE)[2];
    }
#endif

    palSetLine(PORTAB_STATUS_LED);

    uint8_t mask = 1 << (bl->blockNo % 8);
    uint32_t pos = bl->blockNo / 8;
    if (!(wrState.writtenMask[pos] & mask)) {
        wrState.writtenMask[pos] |= mask;
        wrState.numWritten++;

        if ((bl->flags & UF2_FLAG_NOFLASH) || bl->payloadSize > 256 || (bl->targetAddr & 0xff) ||
            !VALID_FLASH_ADDR(bl->targetAddr, bl->payloadSize) || !UID_check) {
            DBG("Skip block at %x", bl->targetAddr);
            // this happens when we're trying to re-flash CURRENT.UF2 file previously
            // copied from a device; we still want to count these blocks to reset properly
        } else {
            DBG("Write block at %x", bl->targetAddr);
            // TODO: wait with writing APP_LOAD_ADDRESS until last block is written
            flash_write(bl->targetAddr, bl->data, bl->payloadSize, failsafe_mode);
        }
    }
    if (wrState.numWritten >= wrState.numBlocks) {
        // wait a little bit before resetting, to avoid Windows transmit error
        // https://github.com/Microsoft/uf2-samd21/issues/11
        // a bit longer than 30ms to avoid Gnome transmit error
        // Actually it feels better to have a little delay, 30ms feels
        // too fast for firmware to really update, 500ms feels more like
        // a realistic time :)
        uf2_timer_start(500);
    } else {
        // if the next block is not received within 500 ms, reset
        uf2_timer_start(500);
    }

    palClearLine(PORTAB_STATUS_LED);

    return 0;
}

/* Check failsafe button */
bool check_failsafe_button(void) {
#ifdef PORTAB_FAILSAFE_BUTTON
    int samples, vote = 0;
    for (samples = 0; samples < 200; samples++) {
        if (palReadLine(PORTAB_FAILSAFE_BUTTON) == PORTAB_FAILSAFE_BUTTON_PRESSED) {
            vote++;
        }
    }
    /* reject a little noise */
    return (vote * 100) > (samples * 90);
#else
    return false;
#endif
}

void ghostfat_init(void) {
    failsafe_mode = check_failsafe_button();
#ifdef USE_CONFIGFILE
    if (!failsafe_mode) {
        cfghtm_size = segmentedFileLength(CONFIGHTM_FILE, CONFIGHTM_SEGMENTS);
    }
#endif
}
