/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: key.c
*
* PROJECT....: EMP
*
* DESCRIPTION: 3x4 keypad task implementation using FreeRTOS queues.
*
*****************************************************************************/

/***************************** Include files *******************************/
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "emp_type.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "key.h"

/*****************************   Variables   *******************************/
extern QueueHandle_t key_queue;

/*****************************   Functions   *******************************/
static INT8U row(INT8U y)
{
  switch(y)
  {
    case 0x01: return 1;
    case 0x02: return 2;
    case 0x04: return 3;
    case 0x08: return 4;
    default:   return 0;
  }
}

static INT8U key_catch(INT8U x, INT8U y)
{
  static const INT8U matrix[3][4] = {
    {'*', '7', '4', '1'},
    {'0', '8', '5', '2'},
    {'#', '9', '6', '3'}
  };

  return matrix[x - 1][y - 1];
}

static BOOLEAN scan_column(INT8U column, INT8U *pch)
{
  INT8U y;

  GPIO_PORTA_DATA_R &= 0xE3; /* Clear PA2, PA3 and PA4 */

  if(column == 1)
    GPIO_PORTA_DATA_R |= 0x10;
  else if(column == 2)
    GPIO_PORTA_DATA_R |= 0x08;
  else
    GPIO_PORTA_DATA_R |= 0x04;

  y = GPIO_PORTE_DATA_R & 0x0F;
  if(y)
  {
    *pch = key_catch(column, row(y));
    return 1;
  }

  return 0;
}

static BOOLEAN scan_keypad(INT8U *pch)
{
  if(scan_column(1, pch))
    return 1;

  if(scan_column(2, pch))
    return 1;

  if(scan_column(3, pch))
    return 1;

  return 0;
}

void key_init(void)
{
  volatile INT32U dummy;

  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA | SYSCTL_RCGC2_GPIOE; // Configure clock for port A and E
  dummy = SYSCTL_RCGC2_R; // dummy read

  GPIO_PORTA_DIR_R |= 0x1C; // Set PA2, PA3 and PA4 as digital out (keypad collumns)
  GPIO_PORTA_DEN_R |= 0x1C; // Enable digital functions on PA2, PA3 and PA4
  GPIO_PORTA_AFSEL_R &= ~0x1C; // Disable alternate peripherals
  GPIO_PORTA_AMSEL_R &= ~0x1C; // Disable analog mode
  GPIO_PORTA_PCTL_R &= 0xFFF000FF; // Clear mux fields in PCTL

  GPIO_PORTE_DIR_R &= ~0x0F; // Set PE0, PE1, PE2, and PE3 as inputs (keypad rows)
  GPIO_PORTE_DEN_R |= 0x0F; // Enable digital functions
  GPIO_PORTE_AFSEL_R &= ~0x0F; // Disable alternate peripherals
  GPIO_PORTE_AMSEL_R &= ~0x0F; // Disable analog mode
  GPIO_PORTE_PCTL_R &= 0xFFFF0000; // Clear mux fields in PCTL
}

BOOLEAN get_keyboard(INT8U *pch)
{
  return (xQueueReceive(key_queue, pch, portMAX_DELAY) == pdTRUE);
}

void key_task(void *pvParameters)
{
  INT8U ch;
  BOOLEAN wait_for_release = 0;

  (void)pvParameters;
  key_init();

  while(1)
  {
    if(!wait_for_release)
    {
      if(scan_keypad(&ch))
      {
        xQueueOverwrite(key_queue, &ch);
        wait_for_release = 1;
      }
    }
    else
    {
      if(!scan_keypad(&ch))
      {
        wait_for_release = 0;
      }
    }

    vTaskDelay(20 / portTICK_RATE_MS);
  }
}
