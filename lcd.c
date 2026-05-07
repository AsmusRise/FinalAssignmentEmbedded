/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: lcd.c
*
* PROJECT....: Coffee Machine
*
* DESCRIPTION: HD44780 16x2 LCD driver (4-bit mode) for EMP board.
*              Low-level code from Assignment 5 solution.
*              Task adapted from RTCS to FreeRTOS.
*
*   Pin mapping:
*     RS  = PD2          EN  = PD3
*     D4  = PC4          D5  = PC5
*     D6  = PC6          D7  = PC7
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
#include "emp_type.h"
#include "lcd.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*****************************    Defines    *******************************/

enum LCD_states
{
  LCD_POWER_UP,
  LCD_INIT,
  LCD_READY,
  LCD_ESC_RECEIVED,
};

/*****************************   Constants   *******************************/

const INT8U LCD_init_sequense[]=
{
  0x30,     // Reset
  0x30,     // Reset
  0x30,     // Reset
  0x20,     // Set 4bit interface
  0x28,     // 2 lines Display
  0x0C,     // Display ON, Cursor OFF, Blink OFF
  0x06,     // Cursor Increment
  0x01,     // Clear Display
  0x02,     // Home
  0xFF      // stop
};

/*****************************   Variables   *******************************/

extern QueueHandle_t lcd_queue;

/*****************************   Functions   *******************************/

void wr_ctrl_LCD_low( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - lower nibble control data
*   Output   : -
*   Function : Write low part of control data to LCD.
******************************************************************************/
{
  INT8U temp;
  volatile int i;

  temp = GPIO_PORTC_DATA_R & 0x0F;
  temp  = temp | ((Ch & 0x0F) << 4);
  GPIO_PORTC_DATA_R  = temp;
  for( i=0; i<1000; i )
    i++;
  GPIO_PORTD_DATA_R &= 0xFB;        // Select Control mode, RS low
  for( i=0; i<1000; i )
    i++;
  GPIO_PORTD_DATA_R |= 0x08;        // Set E High
  for( i=0; i<1000; i )
    i++;
  GPIO_PORTD_DATA_R &= 0xF7;        // Set E Low
  for( i=0; i<1000; i )
    i++;
}

void wr_ctrl_LCD_high( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - full byte, only upper nibble is sent
*   Output   : -
*   Function : Write high part of control data to LCD.
******************************************************************************/
{
  wr_ctrl_LCD_low(( Ch & 0xF0 ) >> 4 );
}

void out_LCD_low( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - lower nibble character data
*   Output   : -
*   Function : Send low part of character to LCD (4-bit data mode).
******************************************************************************/
{
  INT8U temp;

  temp = GPIO_PORTC_DATA_R & 0x0F;
  GPIO_PORTC_DATA_R  = temp | ((Ch & 0x0F) << 4);
  GPIO_PORTD_DATA_R |= 0x04;        // Select data mode, RS high
  GPIO_PORTD_DATA_R |= 0x08;        // Set E High
  GPIO_PORTD_DATA_R &= 0xF7;        // Set E Low
}

void out_LCD_high( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - full byte, only upper nibble is sent
*   Output   : -
*   Function : Send high part of character to LCD (4-bit data mode).
******************************************************************************/
{
  out_LCD_low((Ch & 0xF0) >> 4);
}

void wr_ctrl_LCD( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - command byte
*   Output   : -
*   Function : Write control data to LCD. Handles 8-bit to 4-bit transition.
******************************************************************************/
{
  static INT8U Mode4bit = 0;
  INT16U i;

  wr_ctrl_LCD_high( Ch );
  if( Mode4bit )
  {
    for(i=0; i<1000; i++);
    wr_ctrl_LCD_low( Ch );
  }
  else
  {
    if( (Ch & 0x30) == 0x20 )
      Mode4bit = 1;
  }
}

void clr_LCD()
/*****************************************************************************
*   Input    : -
*   Output   : -
*   Function : Clear LCD.
******************************************************************************/
{
  wr_ctrl_LCD( 0x01 ); 
}

void home_LCD()
/*****************************************************************************
*   Input    : -
*   Output   : -
*   Function : Return cursor to the home position.
******************************************************************************/
{
  wr_ctrl_LCD( 0x02 );
}

void Set_cursor( INT8U Ch )
/*****************************************************************************
*   Input    : New cursor position (DDRAM address | 0x80)
*   Output   : -
*   Function : Place cursor at given position.
******************************************************************************/
{
  wr_ctrl_LCD( Ch );
}

void out_LCD( INT8U Ch )
/*****************************************************************************
*   Input    : Ch - character to write
*   Output   : -
*   Function : Write a character to the LCD (4-bit mode, both nibbles).
******************************************************************************/
{
  INT16U i;

  out_LCD_high( Ch );
  for(i=0; i<1000; i++);
  out_LCD_low( Ch );
}

/*****************************  Public API  ********************************/

INT8U wr_ch_LCD( INT8U Ch )
/*****************************************************************************
*   OBSERVE  : LCD task needs ~20ms to print one character.
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  return( xQueueSend( lcd_queue, &Ch, portMAX_DELAY ));
}

void wr_str_LCD( INT8U *pStr )
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  while( *pStr )
  {
    wr_ch_LCD( *pStr );
    pStr++;
  }
}

void move_LCD( INT8U x, INT8U y )
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  INT8U Pos;

  Pos = y * 0x40 + x;
  Pos |= 0x80;
  wr_ch_LCD( ESC );
  wr_ch_LCD( Pos );
}

/*****************************************************************************
 *  lcd_init: configure GPIO and perform short HD44780 init sequence
 ******************************************************************************/
void lcd_init(void)
{
  volatile int dummy;
  int i, j;

  /* Enable GPIOC and GPIOD */
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOC | SYSCTL_RCGC2_GPIOD;
  dummy = SYSCTL_RCGC2_R; /* short delay */

  /* Configure PC4-PC7 as outputs (D4..D7) */
  GPIO_PORTC_DIR_R |= 0xF0;
  GPIO_PORTC_DEN_R |= 0xF0;

  /* Configure PD2-PD3 as outputs (RS, E) */
  GPIO_PORTD_DIR_R |= 0x0C;
  GPIO_PORTD_DEN_R |= 0x0C;

  /* Clear outputs */
  GPIO_PORTC_DATA_R &= ~0xF0;
  GPIO_PORTD_DATA_R &= ~0x0C;

  /* Run the LCD init sequence synchronously (short busy-wait between commands) */
  for (i = 0; LCD_init_sequense[i] != 0xFF; i++)
  {
    wr_ctrl_LCD(LCD_init_sequense[i]);
    for (j = 0; j < 20000; j++); /* short delay */
  }

  for (j = 0; j < 20000; j++); /* final settle */
}


/*****************************************************************************
 *  lcd_test_task: simple one-shot test that prints "hello"/"world"
 ******************************************************************************/
void lcd_test_task( void *pvParameters )
{
  (void)pvParameters;

  clr_LCD();
  move_LCD(0,0);
  wr_str_LCD((INT8U*)"hello");
  move_LCD(0,1);
  wr_str_LCD((INT8U*)"world");

  /* allow time for lcd_task to process queued characters */
  vTaskDelay(1000 / portTICK_RATE_MS);
  vTaskDelete(NULL);
}

/*****************************  FreeRTOS Task  *****************************/

void lcd_task( void *pvParameters )
/*****************************************************************************
*   Input    : pvParameters (unused)
*   Output   : -
*   Function : FreeRTOS task that initialises the LCD and then processes
*              characters from lcd_queue.
*
*              Send ESC followed by a DDRAM address (0x80|pos) to move cursor.
*              Send ESC followed by '@' to home.
*              Send 0xFF to clear.
*              Anything else is written as a character.
*****************************************************************************/
{
  INT8U ch;
  INT8U LCD_init_idx = 0;
  enum LCD_states state = LCD_READY;

  while(1)
  {
    switch( state )
    {
      case LCD_POWER_UP:
        LCD_init_idx = 0;
        state = LCD_INIT;
        vTaskDelay( 100 / portTICK_RATE_MS );
        break;

      case LCD_INIT:
        if( LCD_init_sequense[LCD_init_idx] != 0xFF )
        {
          wr_ctrl_LCD( LCD_init_sequense[LCD_init_idx++] );
        }
        else
        {
          state = LCD_READY;
        }
        vTaskDelay( 100 / portTICK_RATE_MS );
        break;

      case LCD_READY:
        if( xQueueReceive( lcd_queue, &ch, portMAX_DELAY ) == pdTRUE )
        {
          switch( ch )
          {
            case 0xFF:
              clr_LCD();
              break;
            case ESC:
              state = LCD_ESC_RECEIVED;
              break;
            default:
              out_LCD( ch );
          }
        }
        break;

      case LCD_ESC_RECEIVED:
        if( xQueueReceive( lcd_queue, &ch, portMAX_DELAY ) == pdTRUE )
        {
          if( ch & 0x80 )
          {
            Set_cursor( ch );
          }
          else
          {
            switch( ch )
            {
              case '@':
                home_LCD();
                break;
            }
          }
          state = LCD_READY;
          vTaskDelay( 10 / portTICK_RATE_MS );
        }
        break;
    }
  }
}

/****************************** End Of Module *******************************/
