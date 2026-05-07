/*****************************************************************************
* TESTBENCH.H - Individual component testing
*
* Allows isolated testing of tasks: LED, LCD, UART, Button
* Toggle BUILD_TESTBENCH in main.c to enable/disable
*****************************************************************************/

#ifndef TESTBENCH_H
#define TESTBENCH_H

#include "emp_type.h"

/* Enable individual component tests */
void test_status_led_blink(void *pvParameters);
void test_lcd_display(void *pvParameters);
void test_uart_echo(void *pvParameters);
void test_button_press(void *pvParameters);
void test_color_led(void *pvParameters);

/* Utility for debug output via UART */
void test_debug_printf(const char *fmt, ...);

#endif /* TESTBENCH_H */
