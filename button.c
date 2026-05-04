/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: button.c
*
* PROJECT....: Traffic Light
*
* DESCRIPTION: See module specification file (.h-file).
*****************************************************************************/

/***************************** Include files *******************************/
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "standardTypes.h"
#include "button.h"

//freeRTOS 
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"


extern QueueHandle_t button_queue1;
extern QueueHandle_t button_queue2;

/*****************************    Defines    *******************************/


/*****************************   Constants   *******************************/

/*****************************   Variables   *******************************/

/*****************************   Functions   *******************************/
INT8U button_pushed_1()
/*****************************************************************************
*   Input    : -
*   Output   : TRUE if button is pushed, FALSE otherwise
*   Function : Check if the button (SW1 on PF4) is pushed
******************************************************************************/
{
  return( !(GPIO_PORTF_DATA_R & 0x10) );  // SW at PF4
}



void button_1_Task(void *pvParameters)
{
	//implement while loop
	while(1)
	{
		if(button_pushed_1()) // make this into a task that puts the state into a que. 
		{	
			//debounce
			vTaskDelay(10 / portTICK_RATE_MS);
			if(button_pushed_1())
			{
				xQueueSend(button_queue1, 1, 0);
			}
		}
		//delay task
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

void button_2_Task(void *pvParameters)
{
	//implement while loop
	while(1)
	{	
		if(button_pushed_2())
		{
			//debounce
			vTaskDelay(10 / portTICK_RATE_MS);
			if(button_pushed_2())
			{
				xQueueSend(button_queue2, 1, 0);
			}
		} 
		//delay task
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}


/****************************** End Of Module *******************************/












