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
#include "emp_type.h"
#include "button.h"

//freeRTOS 
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


extern QueueHandle_t button_queue1;
extern QueueHandle_t button_queue2;

/*****************************    Defines    *******************************/


/*****************************   Constants   *******************************/

/*****************************   Variables   *******************************/

/*****************************   Functions   *******************************/
void buttons_init(void)
/*****************************************************************************
*   Input    : -
*   Output   : -
*   Function : Initialize SW1 (PF4) and SW2 (PF0) as digital inputs
*              with internal pull-up resistors.
*              Note: Assumes Port F clock and PF0 unlock already done
*              in LED init.
******************************************************************************/
{
    int dummy;

    // Enable GPIO port F
    SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOF;
    // Dummy read to insert a few cycles after enabling the peripheral
    dummy = SYSCTL_RCGC2_R;

	// Unlock PF0 (commit-locked by default on TM4C123)
    GPIO_PORTF_LOCK_R = 0x4C4F434B;
    GPIO_PORTF_CR_R  |= 0x01;

    // PF0 and PF4 as input (leave other bits unchanged)
    GPIO_PORTF_DIR_R &= ~0x11;

    // Enable pull-up resistors for PF4 (SW1) and PF0 (SW2)
    GPIO_PORTF_PUR_R |= 0x11;

    // Enable digital function for PF0 and PF4 (leave other bits unchanged)
    GPIO_PORTF_DEN_R |= 0x11;
}

INT8U button_pushed_1()
/*****************************************************************************
*   Input    : -
*   Output   : TRUE if button is pushed, FALSE otherwise
*   Function : Check if the button (SW1 on PF4) is pushed
******************************************************************************/
{
  return( !(GPIO_PORTF_DATA_R & 0x10) );  // SW1 at PF4
}

INT8U button_pushed_2()
/*****************************************************************************
*   Input    : -
*   Output   : TRUE if button is pushed, FALSE otherwise
*   Function : Check if the button (SW2 on PF0) is pushed
******************************************************************************/
{
  return( !(GPIO_PORTF_DATA_R & 0x01) );  // SW2 at PF0
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
				xQueueSend(button_queue1, &(INT8U){1}, 0);
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
				xQueueSend(button_queue2, &(INT8U){1}, 0);
			}
		} 
		//delay task
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}


/****************************** End Of Module *******************************/












