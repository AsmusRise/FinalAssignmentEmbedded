/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: leds.c
*
* PROJECT....: ECP
*
* DESCRIPTION: See module specification file (.h-file).
*
* Change Log:
******************************************************************************
* Date    Id    Change
* YYMMDD
* --------------------
* 050128  KA    Module created.
*
*****************************************************************************/

/***************************** Include files *******************************/
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "emp_type.h"
//#include "glob_def.h"
//#include "binary.h"
#include "color_led.h"
#include "uart0.h"

//the way of declaring the queue in other files than the main ig.
extern QueueHandle_t adc_to_uart_queue;
extern QueueHandle_t adc_queue;
extern SemaphoreHandle_t xSemaphore;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;

/*****************************    Defines    *******************************/
#define PF0		0		// Bit 0

/*****************************   Constants   *******************************/

/*****************************   Variables   *******************************/

/*****************************   Functions   *******************************/

void led_init(void)
/*****************************************************************************
*   Input    : 	-
*   Output   : 	-
*   Function : 	
*****************************************************************************/
{
  int dummy;

  // Enable the GPIO port that is used for the on-board LED.
  SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOD | SYSCTL_RCGC2_GPIOF;

  // Do a dummy read to insert a few cycles after enabling the peripheral.
  dummy = SYSCTL_RCGC2_R;

  // Set the direction as output (PF1, PF2 and PF3).
  GPIO_PORTF_DIR_R = 0x0E;

  // Enable the GPIO pins for digital function (PF1, PF2, PF3).
  GPIO_PORTF_DEN_R = 0x0E;

}



void red_led_task(void *pvParameters)
{
	//declare the received value from the queue:
  INT16U received_pot_value = 0;
	led_init();

	while(1)
	{
		// Toggle color LEDs on Port F
    // PORTF refers to what can be seen in the datasheet. If we want to toggle only the red Led
    // which has PF1, we need to toggle 0x02
	if ( xQueueReceive(adc_queue, &received_pot_value, 0))
  {
    
    //this way we convert the received pot value to an integer, ranging from 0-4095, and can delay the 
    //light from 10 to 4105 ms.
    int pot_in_int = received_pot_value;
    int delay_time = 100 + (pot_in_int*900)/4095;
    
    
    GPIO_PORTF_DATA_R ^= 0x02;
    vTaskDelay(delay_time / portTICK_RATE_MS);

  }
  else
  {
    GPIO_PORTF_DATA_R ^= 0x02;
    vTaskDelay(500 / portTICK_RATE_MS);
  }
  }
}


void yellow_led_task(void *pvParameters)

{
	while(1)
	{
    if(xSemaphoreTake(xSemaphore, portMAX_DELAY)==pdTRUE)
    {
      // Toggle LED to state 0x04
      GPIO_PORTF_DATA_R ^= 0x04;
      vTaskDelay(2000 / portTICK_RATE_MS);
      // Toggle it back before handing over semaphore!
      GPIO_PORTF_DATA_R ^= 0x04;
      
      xSemaphoreGive(xSemaphore);

      // CRITICAL FIX: Tell the task to step back in line
      // so other tasks waiting for the semaphore finally get a turn!
      vTaskDelay(10 / portTICK_RATE_MS);
    }
  }
}

void green_led_task(void *pvParameters)

{
	while(1)
	{
    if(xSemaphoreTake(xSemaphore, portMAX_DELAY) ==pdTRUE)
    {
      // Toggle LED to state 0x08
      GPIO_PORTF_DATA_R ^= 0x08;
      vTaskDelay(3000 / portTICK_RATE_MS);
      // Toggle it back before handing over!
      GPIO_PORTF_DATA_R ^= 0x08;
      
      xSemaphoreGive(xSemaphore);

      // CRITICAL FIX: Step back in line!
      vTaskDelay(10 / portTICK_RATE_MS);
    }
  }
}

void led_task(void *pvParameters)
{
  INT16U greenON_RX = 0;
  INT16U yellowON_RX = 0;
  INT16U redON_RX = 0;
  while(1)  
  {
  //GPIO_PORTF_DATA_R ^= 0x08; // green led
  //GPIO_PORTF_DATA_R ^= 0x04; // yellow led
  //GPIO_PORTF_DATA_R ^= 0x02; // red led
    if(xQueuePeek(greenQueue, &greenON_RX, 0)) // 0 instead of ( TickType_t ) 10 ) as i dont want to do anything if nothing is in the queue.
    {
      if(greenON_RX == 0){
        GPIO_PORTF_DATA_R &= ~0x08;
      }
      else
      {
        GPIO_PORTF_DATA_R |= 0x08;
      }
    }

    if(xQueuePeek(yellowQueue, &yellowON_RX, 0)) // 0 instead of ( TickType_t ) 10 ) as i dont want to do anything if nothing is in the queue.
    {
      if(yellowON_RX == 0){
        GPIO_PORTF_DATA_R &= ~0x04;
      }
      else
      {
        GPIO_PORTF_DATA_R |= 0x04;
      }
    }

    if(xQueuePeek(redQueue, &redON_RX, 0)) // 0 instead of ( TickType_t ) 10 ) as i dont want to do anything if nothing is in the queue.
    {
      if(redON_RX == 0){
        GPIO_PORTF_DATA_R &= ~0x02;
      }
      else
      {
        GPIO_PORTF_DATA_R |= 0x02;
      }
    }

    vTaskDelay(10 / portTICK_RATE_MS);
  }

  
}
/****************************** End Of Module *******************************/




