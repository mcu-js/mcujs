/* Sticky uses a USB-UART bridge. Preserve the private console ABI without
 * starting USB-OTG on GPIO19/20 (those pins belong to the microphone). */
#include "usb_cdc.h"
#include "board_config.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
static bool initialized;
static int pending=-1;
void usb_cdc_init(void) {
 const uart_config_t config={.baud_rate=MCUJS_UART_BAUD,.data_bits=UART_DATA_8_BITS,
 .parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,
 .source_clk=UART_SCLK_DEFAULT};
 ESP_ERROR_CHECK(uart_param_config(UART_NUM_0,&config));
 ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0,MCUJS_UART_TX_PIN,MCUJS_UART_RX_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
 ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,1024,0,0,NULL,0));
 initialized=true;
}
void usb_cdc_task(void) {}
bool usb_cdc_connected(void) {return initialized; /* Physical UART has no DTR visibility. */}
bool usb_cdc_available(void) {
 if(pending>=0)return true;
 uint8_t c;
 if(initialized && uart_read_bytes(UART_NUM_0,&c,1,0)==1){pending=c;return true;}
 return false;
}
int usb_cdc_getchar(void) {if(!usb_cdc_available())return -1;int c=pending;pending=-1;return c;}
size_t usb_cdc_read(char *buffer,size_t n) {
 if(!buffer || !n || !initialized)return 0;
 size_t used=0;
 if(pending>=0){buffer[used++]=(char)pending;pending=-1;}
 if(used<n){int got=uart_read_bytes(UART_NUM_0,buffer+used,n-used,0);if(got>0)used+=(size_t)got;}
 return used;
}
size_t usb_cdc_write(const char *data,size_t n) {
 if(!data || !n || !initialized)return 0;
 int written=uart_write_bytes(UART_NUM_0,data,n);return written>0?(size_t)written:0;
}
void usb_cdc_puts(const char *text) {
 if(!text || !initialized)return;
 size_t n=strlen(text),off=0;
 while(off<n){size_t amount=n-off;if(amount>128)amount=128;
  size_t wrote=usb_cdc_write(text+off,amount);if(!wrote)break;off+=wrote;
  if(esp_task_wdt_status(NULL)==ESP_OK)(void)esp_task_wdt_reset();
  if(off<n)vTaskDelay(1);
 }
}
void usb_cdc_putchar(char c){(void)usb_cdc_write(&c,1);}
void usb_cdc_flush(void){if(initialized)(void)uart_wait_tx_done(UART_NUM_0,pdMS_TO_TICKS(100));}
void usb_cdc_reset_usb(uint32_t ms){usb_cdc_flush();vTaskDelay(pdMS_TO_TICKS(ms)?pdMS_TO_TICKS(ms):1);esp_restart();}
