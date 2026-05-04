#include "coffebrewer.h"
#include "queue.h"
#include "emp_type.h"
#include "status_led.h"
#include "color_led.h"
#include "adc.h"
#include "key.h"
#include "lcd.h"
#include "uart0.h"

extern QueueHandle_t button_queue;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;
extern QueueHandle_t key_queue;
extern QueueHandle_t lcd_queue;
extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t adc_queue;
extern QueueHandle_t adc_to_uart_queue;

