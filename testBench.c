/*****************************************************************************
* TESTBENCH.C - Individual component testing
*
* Tests: LED blinking, LCD updates, UART echo, Button presses, Color LEDs
*****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tm4c123gh6pm.h"
#include "emp_type.h"
#include "status_led.h"
#include "color_led.h"
#include "lcd.h"
#include "uart0.h"
#include "key.h"

extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t key_queue;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;

/*****************************************************************************
* test_debug_printf - Send formatted debug string over UART
*****************************************************************************/
void test_debug_printf(const char *fmt, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  uart0_puts_selfmade((INT8U*)buffer);
}

/*****************************************************************************
* test_status_led_blink - Verify status LED flickers on/off every 500ms
*****************************************************************************/
void test_status_led_blink(void *pvParameters)
{
  (void)pvParameters;
  INT32U blink_count = 0;
  char buffer[17];

  status_led_init();

  test_debug_printf("\r\n=== STATUS LED BLINK TEST ===\r\n");
  test_debug_printf("Status LED should toggle every 500ms\r\n");

  clr_LCD();
  move_LCD(0, 0);
  wr_str_LCD((INT8U*)"TEST: Status LED");
  move_LCD(0, 1);
  wr_str_LCD((INT8U*)"Blinking...");

  while(1)
  {
    GPIO_PORTD_DATA_R ^= 0x40;
    test_debug_printf("BLINK #%lu: LED ON\r\n", blink_count);
    move_LCD(0, 1);
    snprintf(buffer, 16, "ON  %lu", blink_count);
    wr_str_LCD((INT8U*)buffer);
    
    vTaskDelay(500 / portTICK_RATE_MS);

    GPIO_PORTD_DATA_R ^= 0x40;
    test_debug_printf("BLINK #%lu: LED OFF\r\n", blink_count);
    move_LCD(0, 1);
    snprintf(buffer, 16, "OFF %lu", blink_count);
    wr_str_LCD((INT8U*)buffer);
    
    blink_count++;
    vTaskDelay(500 / portTICK_RATE_MS);
  }
}

/*****************************************************************************
* test_lcd_display - Cycle through LCD test patterns
*****************************************************************************/
void test_lcd_display(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  INT32U count = 0;

  test_debug_printf("\r\n=== LCD DISPLAY TEST ===\r\n");
  test_debug_printf("LCD should show cycling patterns every 1 second\r\n");

  clr_LCD();

  while(1)
  {
    clr_LCD();
    move_LCD(0, 0);
    wr_str_LCD((INT8U*)"LCD TEST");
    move_LCD(0, 1);
    
    char line2[17];
    snprintf(line2, sizeof(line2), "Count: %lu", count);
    wr_str_LCD((INT8U*)line2);

    test_debug_printf("LCD update #%lu: '%s' / '%s'\r\n", count, "LCD TEST", line2);

    count++;
    vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_RATE_MS);
  }
}

/*****************************************************************************
* test_uart_echo - Echo received UART characters back, show on LCD
*****************************************************************************/
void test_uart_echo(void *pvParameters)
{
  (void)pvParameters;
  INT8U ch;
  INT32U count = 0;

  test_debug_printf("\r\n=== UART ECHO TEST ===\r\n");
  test_debug_printf("Send characters - they will echo back\r\n");

  clr_LCD();
  move_LCD(0, 0);
  wr_str_LCD((INT8U*)"UART ECHO TEST");

  while(1)
  {
    if(xQueueReceive(uart_rx_queue, &ch, 100 / portTICK_RATE_MS) == pdTRUE)
    {
      count++;
      test_debug_printf("RX #%lu: 0x%02X ('%c')\r\n", count, ch, (ch >= 32 && ch < 127) ? ch : '?');
      
      /* Echo back */
      xQueueSend(uart_tx_queue, &ch, 0);

      /* Show on LCD */
      move_LCD(0, 1);
      char echo_display[17];
      snprintf(echo_display, sizeof(echo_display), "RX: 0x%02X", ch);
      wr_str_LCD((INT8U*)echo_display);
    }
  }
}

/*****************************************************************************
* test_button_press - Detect and display button/key presses
*****************************************************************************/
void test_button_press(void *pvParameters)
{
  (void)pvParameters;
  INT8U key_val;
  INT32U press_count = 0;

  test_debug_printf("\r\n=== BUTTON/KEY PRESS TEST ===\r\n");
  test_debug_printf("Press keypad buttons - they will appear below\r\n");

  clr_LCD();
  move_LCD(0, 0);
  wr_str_LCD((INT8U*)"KEY PRESS TEST");

  while(1)
  {
    if(xQueueReceive(key_queue, &key_val, 100 / portTICK_RATE_MS) == pdTRUE)
    {
      press_count++;
      test_debug_printf("KEY PRESS #%lu: '%c' (0x%02X)\r\n", press_count, key_val, key_val);

      move_LCD(0, 1);
      char key_display[17];
      snprintf(key_display, sizeof(key_display), "Key: %c #%lu", key_val, press_count);
      wr_str_LCD((INT8U*)key_display);
    }
  }
}

/*****************************************************************************
* test_color_led - Cycle through color LEDs (Green, Yellow, Red)
*****************************************************************************/
void test_color_led(void *pvParameters)
{
  (void)pvParameters;
  INT32U cycle = 0;
  INT16U led_on_value = 1;
  INT16U led_off_value = 0;

  test_debug_printf("\r\n=== COLOR LED TEST ===\r\n");
  test_debug_printf("LEDs should cycle: Green -> Yellow -> Red -> Off, repeat\r\n");

  clr_LCD();
  move_LCD(0, 0);
  wr_str_LCD((INT8U*)"COLOR LED TEST");

  while(1)
  {
    /* Green ON */
    xQueueSend(greenQueue, &led_on_value, portMAX_DELAY);
    test_debug_printf("CYCLE #%lu: GREEN ON\r\n", cycle);
    move_LCD(0, 1);
    wr_str_LCD((INT8U*)"GREEN ON");
    vTaskDelay(500 / portTICK_RATE_MS);

    /* Yellow ON */
    xQueueSend(greenQueue, &led_off_value, portMAX_DELAY);
    xQueueSend(yellowQueue, &led_on_value, portMAX_DELAY);
    test_debug_printf("CYCLE #%lu: YELLOW ON\r\n", cycle);
    move_LCD(0, 1);
    wr_str_LCD((INT8U*)"YELLOW ON");
    vTaskDelay(500 / portTICK_RATE_MS);

    /* Red ON */
    xQueueSend(yellowQueue, &led_off_value, portMAX_DELAY);
    xQueueSend(redQueue, &led_on_value, portMAX_DELAY);
    test_debug_printf("CYCLE #%lu: RED ON\r\n", cycle);
    move_LCD(0, 1);
    wr_str_LCD((INT8U*)"RED ON");
    vTaskDelay(500 / portTICK_RATE_MS);

    /* All OFF */
    xQueueSend(redQueue, &led_off_value, portMAX_DELAY);
    test_debug_printf("CYCLE #%lu: ALL OFF\r\n", cycle);
    move_LCD(0, 1);
    wr_str_LCD((INT8U*)"ALL OFF");
    vTaskDelay(500 / portTICK_RATE_MS);

    cycle++;
  }
}
