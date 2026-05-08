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
extern SemaphoreHandle_t xSemaphore;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;

/*****************************    Defines    *******************************/
#define PF0		0		// Bit 0

/*****************************   Constants   *******************************/

/*****************************   Variables   *******************************/

/*****************************   Functions   *******************************/

//function to turn all the leds off
void leds_off(void)
{
  GPIO_PORTF_DATA_R &= ~0x0E; // turn off all leds (PF1, PF2 and PF3)
}

void leds_on(void)
{
  GPIO_PORTF_DATA_R |= 0x0E; // turn on all leds (PF1, PF2 and PF3)
}

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




