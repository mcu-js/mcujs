/*
 * mcujs - FatFs Configuration
 * 
 * Custom configuration for ChaN's FatFs library
 * Enables Long Filename (LFN) support for filenames like index.js
 */

#ifndef MCUJS_FFCONF_H
#define MCUJS_FFCONF_H

#define FFCONF_DEF 80386  /* Revision ID - must match FF_DEFINED in ff.h */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY  0
/* Read/Write mode - we need to write files */

#define FF_FS_MINIMIZE  0
/* Full API - we need all file operations */

#define FF_USE_FIND     0
/* Disable filtered directory read - not needed */

#define FF_USE_MKFS     1
/* Enable f_mkfs() for formatting new filesystems */

#define FF_USE_FASTSEEK 0
/* Disable fast seek - not needed for our use case */

#define FF_USE_EXPAND   0
/* Disable f_expand - not needed */

#define FF_USE_CHMOD    0
/* Disable chmod/utime - Pico has no RTC */

#define FF_USE_LABEL    1
/* Enable volume label functions for MCUJS label */

#define FF_USE_FORWARD  0
/* Disable f_forward - not needed */

#define FF_USE_STRFUNC  0
/* Disable string functions - we use our own */

#define FF_PRINT_LLI    0
#define FF_PRINT_FLOAT  0
#define FF_STRF_ENCODE  0

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE    437
/* U.S. code page - standard for FAT */

#define FF_USE_LFN      2
/* Enable LFN with dynamic working buffer on STACK
 * 0: Disable LFN
 * 1: Enable LFN with static buffer (not thread-safe)
 * 2: Enable LFN with stack buffer (our choice - no heap needed)
 * 3: Enable LFN with heap buffer
 */

#define FF_MAX_LFN      255
/* Maximum LFN length - full support for long filenames */

#define FF_LFN_UNICODE  2
/* UTF-8 encoding on API
 * 0: ANSI/OEM
 * 1: UTF-16
 * 2: UTF-8 (our choice - JavaScript strings are UTF-8)
 * 3: UTF-32
 */

#define FF_LFN_BUF      255
#define FF_SFN_BUF      12
/* Buffer sizes for FILINFO structure */

#define FF_FS_RPATH     0
/* Disable relative path - we handle paths in JS layer */

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES      1
/* Single volume - flash filesystem only */

#define FF_STR_VOLUME_ID    0
/* Disable string volume IDs - not needed */

#define FF_MULTI_PARTITION  0
/* Single partition per drive */

#define FF_MIN_SS       512
#define FF_MAX_SS       512
/* Fixed 512-byte sectors - standard FAT sector size */

#define FF_LBA64        0
/* Disable 64-bit LBA - our flash is small */

#define FF_MIN_GPT      0x10000000
/* Not used since FF_LBA64 is 0 */

#define FF_USE_TRIM     0
/* Disable TRIM - flash doesn't support it */

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY      0
/* Normal buffer configuration - each file has its own buffer */

#define FF_FS_EXFAT     0
/* Disable exFAT - FAT12/16 is sufficient for our small filesystem */

#define FF_FS_NORTC     1
/* No RTC - Pico doesn't have one by default */

#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2024
/* Fixed timestamp when no RTC: 2024-01-01 */

#define FF_FS_NOFSINFO  0
/* Trust FSINFO for free cluster count */

#define FF_FS_LOCK      0
/* Disable file lock - single-threaded environment */

#define FF_FS_REENTRANT 0
/* Disable re-entrancy - single-threaded environment */

#define FF_FS_TIMEOUT   1000
/* Not used since FF_FS_REENTRANT is 0 */

#endif /* MCUJS_FFCONF_H */
