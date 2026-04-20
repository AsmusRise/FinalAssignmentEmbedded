

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
#include "status_led.h"
#include "color_led.h"
#include "adc.h"

#include "uart0.h"

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
  init_systick();
  status_led_init();
  led_init();
  uart0_init(9600, 8, 1, 'n');
  

}

QueueHandle_t adc_queue;
QueueHandle_t adc_to_uart_queue;
SemaphoreHandle_t xSemaphore = NULL;
QueueHandle_t button_queue;


int main(void)
{
    setupHardware();
    //create a queue cabable of holding 1 INT16 U 
    adc_queue = xQueueCreate(1, sizeof(INT16U));
    adc_to_uart_queue = xQueueCreate(1, sizeof(INT16U));
    button_queue = xQueueCreate(1, sizeof(INT16U));
    
    //create the mutex
    xSemaphore = xSemaphoreCreateMutex();

    xTaskCreate( status_led_task, "Status_led", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( adc_task, "ADC", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL );
    xTaskCreate( red_led_task, "red", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);

    xTaskCreate( green_led_task, "green", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);
    xTaskCreate( yellow_led_task, "yellow", USERTASK_STACK_SIZE, NULL, LOW_PRIO, NULL);

    xTaskCreate(uartTask, "uartADC", 256, NULL, LOW_PRIO, NULL );
    
    vTaskStartScheduler();
	return 0;
}
