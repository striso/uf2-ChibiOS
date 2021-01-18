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
    {.name = "CONFIG  UF2"},
    {.name = "CONFIG  HTM"},
};
#define NUM_INFO (int)(sizeof(info) / sizeof(info[0]))
#define START_CUSTOM_FILES (NUM_INFO - 3)

#define UF2_INDEX START_CUSTOM_FILES
#define UF2_SIZE (BOARD_FLASH_SIZE * 2)
#define UF2_SECTORS (UF2_SIZE / 512)
#define UF2_FIRST_SECTOR (START_CUSTOM_FILES)
#define UF2_LAST_SECTOR (uint32_t)(UF2_FIRST_SECTOR + UF2_SECTORS - 1)

#define CFGBIN_INDEX (START_CUSTOM_FILES + 1)
#define CFGBIN_SIZE (128 * 1024 * 2)
#define CFGBIN_SECTORS (CFGBIN_SIZE / 512)
#define CFGBIN_FIRST_SECTOR (UF2_LAST_SECTOR + 1)
#define CFGBIN_LAST_SECTOR (CFGBIN_FIRST_SECTOR + CFGBIN_SECTORS - 1)

size_t cfghtm_size;
#define CFGHTM_INDEX (START_CUSTOM_FILES + 2)
#define CFGHTM_SIZE cfghtm_size
#define CFGHTM_SECTORS ((CFGHTM_SIZE + 511) / 512)
#define CFGHTM_FIRST_SECTOR (CFGBIN_LAST_SECTOR + 1)
#define CFGHTM_LAST_SECTOR (CFGHTM_FIRST_SECTOR + CFGHTM_SECTORS - 1)

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
            } else if (CFGBIN_FIRST_SECTOR <= sector && sector <= CFGBIN_LAST_SECTOR) {
                ((uint16_t *)(void *)data)[i] = (sector == CFGBIN_LAST_SECTOR) ? 0xffff : v + 1;
            } else if (CFGHTM_FIRST_SECTOR <= sector && sector <= CFGHTM_LAST_SECTOR) {
                ((uint16_t *)(void *)data)[i] = (sector == CFGHTM_LAST_SECTOR) ? 0xffff : v + 1;
            }
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
                const struct TextFile *inf = &info[i];
                if (i < START_CUSTOM_FILES) {
                    d->size = inf->content ? fileLength(inf->content) : 0;
                    d->startCluster = i + CLUSTER_OFFSET;
                } else if (i == UF2_INDEX) {
                    d->size = UF2_SIZE;
                    d->startCluster = UF2_FIRST_SECTOR + CLUSTER_OFFSET;
                } else if (i == CFGBIN_INDEX) {
                    d->size = CFGBIN_SIZE;
                    d->startCluster = CFGBIN_FIRST_SECTOR + CLUSTER_OFFSET;
                } else if (i == CFGHTM_INDEX) {
                    d->size = CFGHTM_SIZE;
                    d->startCluster = CFGHTM_FIRST_SECTOR + CLUSTER_OFFSET;
                }
                padded_memcpy(d->name, inf->name, 11);
            }
        }
    } else {
        // Send file contents
        sectionIdx -= START_CLUSTERS;
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
            } else if (sectionIdx <= CFGBIN_LAST_SECTOR) {
                // Send CONFIG.UF2
                uint32_t blockNo = sectionIdx - CFGBIN_FIRST_SECTOR;
                uint32_t addr = blockNo * 256 + 0x08020000;
                UF2_Block *bl = (void *)data;
                bl->magicStart0 = UF2_MAGIC_START0;
                bl->magicStart1 = UF2_MAGIC_START1;
                bl->flags = UF2_FLAG_FAMILYID_PRESENT;
                bl->targetAddr = addr;
                bl->payloadSize = 256;
                bl->blockNo = blockNo;
                bl->numBlocks = CFGBIN_SIZE / 256;
                bl->familyID = UF2_FAMILY;
                bl->magicEnd = UF2_MAGIC_END;

                memcpy(bl->data, (void *)addr, bl->payloadSize);
            } else if (sectionIdx <= CFGHTM_LAST_SECTOR) {
                // Send CONFIG.HTM
                uint32_t blockNo = sectionIdx - CFGHTM_FIRST_SECTOR;
                segmentedFileGetSector(CONFIGHTM_FILE, CONFIGHTM_SEGMENTS, blockNo, data);
            }
        }
    }

    return 0;
}

static void write_block_core(uint32_t block_no, const uint8_t *data, bool quiet,
                             WriteState *state) {
    const UF2_Block *bl = (const void *)data;

    (void)block_no;

    // DBG("Write magic: %x", bl->magicStart0);

    if (!is_uf2_block(bl) || !UF2_IS_MY_FAMILY(bl)) {
        return;
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
    if ((bl->flags & UF2_FLAG_NOFLASH) || bl->payloadSize > 256 || (bl->targetAddr & 0xff) ||
        !VALID_FLASH_ADDR(bl->targetAddr, bl->payloadSize) || !UID_check) {
        DBG("Skip block at %x", bl->targetAddr);
        // this happens when we're trying to re-flash CURRENT.UF2 file previously
        // copied from a device; we still want to count these blocks to reset properly
    } else {
        // logval("write block at", bl->targetAddr);
        DBG("Write block at %x", bl->targetAddr);
        // TODO: wait with writing APP_LOAD_ADDRESS until last block is written
        flash_write(bl->targetAddr, bl->data, bl->payloadSize);
    }
    palClearLine(PORTAB_STATUS_LED);

    bool isSet = false;

    if (state && bl->numBlocks) {
        if (state->numBlocks != bl->numBlocks) {
            if (bl->numBlocks >= MAX_BLOCKS || state->numBlocks)
                state->numBlocks = 0xffffffff;
            else
                state->numBlocks = bl->numBlocks;
        }
        if (bl->blockNo < MAX_BLOCKS) {
            uint8_t mask = 1 << (bl->blockNo % 8);
            uint32_t pos = bl->blockNo / 8;
            if (!(state->writtenMask[pos] & mask)) {
                // logval("incr", state->numWritten);
                state->writtenMask[pos] |= mask;
                state->numWritten++;
            }
            if (state->numWritten >= state->numBlocks) {
                // wait a little bit before resetting, to avoid Windows transmit error
                // https://github.com/Microsoft/uf2-samd21/issues/11
                if (!quiet) {
                    uf2_timer_start(30);
                    isSet = true;
                }
            }
        }
        // DBG("wr %d=%d (of %d)", state->numWritten, bl->blockNo, bl->numBlocks);
    }

    // if the next block is not received within 500 ms, reset
    if (!isSet && !quiet) {
        uf2_timer_start(500);
    }
}

WriteState wrState;

int write_block(uint32_t lba, const uint8_t *copy_from) {
    write_block_core(lba, copy_from, false, &wrState);
    return 0;
}

void ghostfat_init(void) {
    cfghtm_size = segmentedFileLength(CONFIGHTM_FILE, CONFIGHTM_SEGMENTS);
}
