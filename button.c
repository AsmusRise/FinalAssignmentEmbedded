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



INT8U button_pushed_2()
/*****************************************************************************
*   Input    : -
*   Output   : TRUE if button is pushed, FALSE otherwise
*   Function : Check if the button (SW1 on PF4) is pushed
******************************************************************************/
{
  return( !(GPIO_PORTF_DATA_R & 0x01) );  // SW at PF4 0001 0000
}


INT8U select_button(void)
/*****************************************************************************
*   Input    : -
*   Output   : Button event (IDLE, SINGLE_PRESS, DOUBLE_PRESS, or LONG_PRESS)
*   Function : Detect and classify button press events using state machine
******************************************************************************/
{
  static INT8U  button_state = IDLE;
  static INT16U button_timer;
         INT8U  button_event = IDLE;

  switch( button_state )
  {
    case IDLE:
	    if( button_pushed() )		            // if button pushed
	    {
	        button_state = FIRST_PRESS;
		    button_timer = TIM_2_SEC;		    // start timer = 2 sec;
	    }
	    break;
    case FIRST_PRESS:
	    if( ! --button_timer )			        // if timeout
	    {
	        button_state = LONG_PRESS;
		    button_event = LONG_PRESS;
	    }
	    else
	    {
	        if( !button_pushed() )	                // if button released
			{
		        button_state = FIRST_RELEASE;
			    button_timer = TIM_100_MSEC;	    // start timer = 100 milli sec;
		    }
	    }
	    break;
    case FIRST_RELEASE:
	    if( ! --button_timer )			        // if timeout
	    {
	        button_state = IDLE;
		    button_event = SINGLE_PRESS;
	    }
	    else
	    {
	        if( button_pushed() )		            // if button pressed
			{
		         button_state = SECOND_PRESS;
			     button_timer = TIM_2_SEC;		    // start timer = 2 sec;
	        }
	    }
	    break;
    case SECOND_PRESS:
	    if( ! --button_timer )			        // if timeout
	    {
	        button_state = LONG_PRESS;
		    button_event = LONG_PRESS;
	    }
	    else
	    {
	        if( !button_pushed() )				    // if button released
			{
		          button_state = IDLE;
			      button_event = DOUBLE_PRESS;
	        }
	    }
	    break;
    case LONG_PRESS:
        if( !button_pushed() )					// if button released
            button_state = IDLE;
	    break;
    default:
        break;
  }
  return( button_event );
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












