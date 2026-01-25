/*
 * mcujs - USB Descriptors
 * 
 * USB CDC + MSC + HID composite device descriptors
 * Based on TinyUSB composite device example
 */

#include "tusb.h"
#include "pico/unique_id.h"
#include "board_config.h"
#include <string.h>
#include <stdio.h>

/*--------------------------------------------------------------------
 * HID Report Descriptor - Keyboard + Mouse Composite
 *--------------------------------------------------------------------*/
#if CFG_TUD_HID

/* Report IDs for composite HID */
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_CONSUMER 3

static uint8_t const desc_hid_report[] = {
    /* Keyboard */
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    /* Mouse */
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    /* Consumer Control (media keys) */
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER)),
};

/* Invoked when received GET HID REPORT DESCRIPTOR */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

#endif /* CFG_TUD_HID */

/*--------------------------------------------------------------------
 * Device Descriptors
 *--------------------------------------------------------------------*/

/* USB VID/PID - Use Raspberry Pi's VID with a test PID */
#ifndef MCUJS_USB_VID
#define MCUJS_USB_VID 0x2E8A  /* Raspberry Pi */
#endif
#ifndef MCUJS_USB_PID
#define MCUJS_USB_PID 0x000A  /* Pico SDK CDC */
#endif

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    
    /* Composite device uses Interface Association Descriptor (IAD) */
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = MCUJS_USB_VID,
    .idProduct          = MCUJS_USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

/*--------------------------------------------------------------------
 * Configuration Descriptor
 *--------------------------------------------------------------------*/

/* Interface numbers */
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
#if CFG_TUD_MSC
    ITF_NUM_MSC,
#endif
#if CFG_TUD_HID
    ITF_NUM_HID,
#endif
    ITF_NUM_TOTAL
};

/* Endpoint addresses */
#define EPNUM_CDC_NOTIF   0x81  /* EP 1 IN - CDC notification */
#define EPNUM_CDC_OUT     0x02  /* EP 2 OUT - CDC data */
#define EPNUM_CDC_IN      0x82  /* EP 2 IN - CDC data */
#define EPNUM_MSC_OUT     0x03  /* EP 3 OUT - MSC data */
#define EPNUM_MSC_IN      0x83  /* EP 3 IN - MSC data */
#define EPNUM_HID_IN      0x84  /* EP 4 IN - HID data */

/* Calculate total config descriptor length */
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + \
                           (CFG_TUD_MSC ? TUD_MSC_DESC_LEN : 0) + \
                           (CFG_TUD_HID ? TUD_HID_DESC_LEN : 0))

static uint8_t const desc_configuration[] = {
    /* Config descriptor: config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),
    
    /* CDC descriptor: interface number, string index, notification EP & size, data EP out, data EP in, data EP size */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    
#if CFG_TUD_MSC
    /* MSC descriptor: interface number, string index, EP out, EP in, EP size */
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
#endif

#if CFG_TUD_HID
    /* HID descriptor: interface number, string index, protocol, report desc len, EP in, EP size, polling interval */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 6, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10),
#endif
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

/*--------------------------------------------------------------------
 * String Descriptors
 *--------------------------------------------------------------------*/

/* String descriptor indices */
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC_INTERFACE,
    STRID_MSC_INTERFACE,
    STRID_HID_INTERFACE,
};

/* Strings array */
static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},  /* 0: Supported language is English (0x0409) */
    "mcujs",                      /* 1: Manufacturer */
    "mcujs Runtime",              /* 2: Product (runtime formatted) */
    NULL,                         /* 3: Serial number (generated at runtime) */
    "mcujs Serial Console",       /* 4: CDC Interface */
    "mcujs Flash Storage",        /* 5: MSC Interface */
    "mcujs Keyboard/Mouse",       /* 6: HID Interface */
};

/* Buffer for string descriptor */
static uint16_t desc_str_buf[32 + 1];

/* Serial number string buffer */
static char serial_str[17];
static bool serial_generated = false;

/* Product string buffer */
static char product_str[64];
static bool product_generated = false;

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;
    const char *str;
    
    if (index == STRID_LANGID) {
        /* Language ID */
        memcpy(&desc_str_buf[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == STRID_SERIAL) {
        /* Generate serial number from unique board ID */
        if (!serial_generated) {
            pico_unique_board_id_t id;
            pico_get_unique_board_id(&id);
            for (int i = 0; i < 8; i++) {
                sprintf(&serial_str[i * 2], "%02X", id.id[i]);
            }
            serial_str[16] = '\0';
            serial_generated = true;
        }
        str = serial_str;
        chr_count = 16;
        for (size_t i = 0; i < chr_count; i++) {
            desc_str_buf[1 + i] = str[i];
        }
    } else if (index == STRID_PRODUCT) {
        if (!product_generated) {
            snprintf(product_str, sizeof(product_str),
                     "mcujs Runtime");
            product_generated = true;
        }
        str = product_str;
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (size_t i = 0; i < chr_count; i++) {
            desc_str_buf[1 + i] = str[i];
        }
    } else {
        /* Other strings */
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        
        str = string_desc_arr[index];
        if (str == NULL) {
            return NULL;
        }
        
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }
        
        /* Convert ASCII to UTF-16 */
        for (size_t i = 0; i < chr_count; i++) {
            desc_str_buf[1 + i] = str[i];
        }
    }
    
    /* First word: length and descriptor type */
    desc_str_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    
    return desc_str_buf;
}
