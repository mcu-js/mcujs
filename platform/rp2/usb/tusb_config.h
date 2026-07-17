/*
 * mcujs - TinyUSB Configuration
 * 
 * Configures USB device (CDC for now, MSC later)
 * Based on Pico SDK and CircuitPython patterns
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------
 * Common Configuration
 *--------------------------------------------------------------------*/

/* Defined by compiler in Pico SDK builds */
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#define CFG_TUSB_OS OPT_OS_PICO

/* Enable debug output on level 0 (none) */
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

/* Memory section and alignment */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

/*--------------------------------------------------------------------
 * Device Configuration
 *--------------------------------------------------------------------*/

#define CFG_TUD_ENABLED 1

/* Use RP2040 USB port 0 in device mode */
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE)

#define CFG_TUD_ENDPOINT0_SIZE 64

/*--------------------------------------------------------------------
 * Class Configuration
 *--------------------------------------------------------------------*/

/* CDC - Serial communication */
#define CFG_TUD_CDC 1
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256

/* MSC - Mass Storage */
#define CFG_TUD_MSC 1
#define CFG_TUD_MSC_EP_BUFSIZE 512

/* HID - Human Interface Device (Keyboard/Mouse) */
#define CFG_TUD_HID 1
#define CFG_TUD_HID_EP_BUFSIZE 16

/* Unused classes */
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */
