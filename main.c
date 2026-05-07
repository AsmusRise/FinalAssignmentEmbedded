

/**
 * main.c
 */
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "emp_type.h"
#include "systick_frt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "color_led.h"
#include "encoder.h"
#include "key.h"
#include "logger.h"
#include "lcd.h"
#include "uart0.h"
#include "button.h"
#include "coffebrewer.h"
#include "testBench.h"

/* ===== TESTBENCH TOGGLE ===== 
 * Set BUILD_TESTBENCH to 1 to enable component tests
 * Set BUILD_TESTBENCH to 0 to run normal coffee brewer application
 */
#define BUILD_TESTBENCH 0

#define USERTASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define IDLE_PRIO 0
#define LOW_PRIO  1
#define MED_PRIO  2
#define HIGH_PRIO 3

static void setupHardware(void)
/*****************************************************************************
*   Input    :  -
*   Output   :  -
*   Function :
*****************************************************************************/
{
  // TODO: Put hardware configuration and initialisation in here

  // Warning: If you do not initialize the hardware clock, the timings will be inaccurate
  
  status_led_init();
  init_systick();
  led_init();
  uart0_init(9600, 8, 1, 'n');
  lcd_init();
}

SemaphoreHandle_t xSemaphore = NULL;

QueueHandle_t button_queue1;
QueueHandle_t button_queue2;
QueueHandle_t greenQueue;
QueueHandle_t yellowQueue;
QueueHandle_t redQueue;
QueueHandle_t key_queue;
QueueHandle_t encoder_queue;
QueueHandle_t lcd_queue;
QueueHandle_t uart_tx_queue;
QueueHandle_t uart_rx_queue;
QueueHandle_t transaction_queue;
QueueHandle_t timerCommandQueue;
SemaphoreHandle_t timer1Semaphore;
SemaphoreHandle_t timer2Semaphore;
SemaphoreHandle_t timer3Semaphore;


int main(void)
{
    setupHardware();
    //create a queue cabable of holding 1 INT16 U

    button_queue1 = xQueueCreate(1, sizeof(INT16U));
    button_queue2 = xQueueCreate(1, sizeof(INT16U));
    greenQueue = xQueueCreate(1, sizeof(INT16U));
    yellowQueue = xQueueCreate(1, sizeof(INT16U));
    redQueue = xQueueCreate(1, sizeof(INT16U));
    key_queue = xQueueCreate(1, sizeof(INT8U));
    encoder_queue = xQueueCreate(1, sizeof(INT8U));
    
    lcd_queue = xQueueCreate(128, sizeof(INT8U));
    uart_tx_queue = xQueueCreate(128, sizeof(INT8U));
    uart_rx_queue = xQueueCreate(128, sizeof(INT8U));
    transaction_queue = xQueueCreate(1, sizeof(transaction_t));
    timerCommandQueue = xQueueCreate(8, sizeof(timer_command_t));

    //create the mutex
    xSemaphore = xSemaphoreCreateMutex();
    timer1Semaphore = xSemaphoreCreateBinary();
    timer2Semaphore = xSemaphoreCreateBinary();
    timer3Semaphore = xSemaphoreCreateBinary();

#if BUILD_TESTBENCH == 0
    /* ===== NORMAL MODE: Full Coffee Brewer Application ===== */
    xTaskCreate( status_led_task, "status_led", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    xTaskCreate( encoder_task, "encoder", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    xTaskCreate( key_task, "key", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);

    xTaskCreate( lcd_task, "LCD", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( uart_tx_task, "UART_TX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( uart_rx_task, "UART_RX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );

    //button tasks
    xTaskCreate( button_1_Task, "Button1", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( button_2_Task, "Button2", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );

    // tasks for the coffebrewer
    xTaskCreate( timer_task, "timer", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( coffebrewer_task, "coffebrewer", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );

    // logging task
    xTaskCreate( log_task, "logger", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );

#else
    /* ===== TESTBENCH MODE: Individual Component Tests ===== 
     * Uncomment the test you want to run:
     */
    
    /* Test 1: Status LED should blink every 500ms */
    xTaskCreate( test_status_led_blink, "TEST_LED", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    
    /* Test 2: LCD should update with counter every 1 second */
    // xTaskCreate( test_lcd_display, "TEST_LCD", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    
    /* Test 3: UART echo - send characters, they echo back */
    // xTaskCreate( test_uart_echo, "TEST_UART", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    // xTaskCreate( uart_tx_task, "UART_TX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    // xTaskCreate( uart_rx_task, "UART_RX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    
    /* Test 4: Button/Key presses */
    // xTaskCreate( test_button_press, "TEST_KEY", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    // xTaskCreate( key_task, "key", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    
    /* Test 5: Color LEDs cycle through Green -> Yellow -> Red */
    // xTaskCreate( test_color_led, "TEST_CLED", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    // xTaskCreate( led_task, "LED_TASK", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    
    /* Support tasks (required for most tests) */
    xTaskCreate( uart_tx_task, "UART_TX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( uart_rx_task, "UART_RX", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
#endif

    vTaskStartScheduler();
	return 0;
}
