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

/**
 * @file    ghostdisk.h
 * @brief   Virtual block devise driver header.
 *
 * @addtogroup ghostdisk
 * @{
 */

#ifndef GHOSTDISK_H_
#define GHOSTDISK_H_

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

typedef struct GhostDisk GhostDisk;

/**
 *
 */
#define _ghostdisk_device_data                                                \
  _base_block_device_data                                                   \
  uint32_t      blk_size;                                                   \
  uint32_t      blk_num;                                                    \
  bool          readonly;

/**
 *
 */
struct GhostDisk {
  /** @brief Virtual Methods Table.*/
  const struct BaseBlockDeviceVMT *vmt;
  _ghostdisk_device_data
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void ghostdiskObjectInit(GhostDisk *rdp);
  void ghostdiskStart(GhostDisk *rdp, uint32_t blksize,
                    uint32_t blknum, bool readonly);
  void ghostdiskStop(GhostDisk *rdp);
#ifdef __cplusplus
}
#endif

#endif /* GHOSTDISK_H_ */

/** @} */
