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
#include "uart0.h"

/* Helper: send a byte as two hex ASCII characters over UART for debugging */
static void uart_puthex(INT8U v)
{
  INT8U high = (v >> 4) & 0x0F;
  INT8U low = v & 0x0F;
  
  if (high < 10) uart0_putc((INT8U)('0' + high));
  else uart0_putc((INT8U)('A' + high - 10));
  
  if (low < 10) uart0_putc((INT8U)('0' + low));
  else uart0_putc((INT8U)('A' + low - 10));
}

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

  /* Enable GPIOC and GPIOD clocks */
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOC | SYSCTL_RCGC2_GPIOD;
  dummy = SYSCTL_RCGC2_R; /* delay after clock enable */
  dummy = SYSCTL_RCGC2_R; /* extra delay for stability */

  /* Configure PC4-PC7 as outputs (D4..D7) */
  GPIO_PORTC_DIR_R |= 0xF0;
  GPIO_PORTC_DEN_R |= 0xF0;

  /* Configure PD2-PD3 as outputs (RS, E) */
  GPIO_PORTD_DIR_R |= 0x0C;
  GPIO_PORTD_DEN_R |= 0x0C;

  /* Clear outputs to safe state */
  GPIO_PORTC_DATA_R &= ~0xF0;
  GPIO_PORTD_DATA_R &= ~0x0C;

  /* LCD init sequence (wr_ctrl_LCD) is handled by lcd_task
     with FreeRTOS delays, not busy-wait here */
}


/*****************************************************************************
 *  lcd_test_task: simple one-shot test that prints "hello"/"world"
 ******************************************************************************/
void lcd_test_task( void *pvParameters )
{
  (void)pvParameters;

  /* Give lcd_task time to initialize */
  uart0_putc('A');  /* debug: task started */
  vTaskDelay(2000 / portTICK_RATE_MS);

  uart0_putc('B');  /* debug: about to clr_LCD */
  clr_LCD();
  vTaskDelay(100 / portTICK_RATE_MS);

  uart0_putc('C');  /* debug: about to move(0,0) */
  move_LCD(0,0);
  vTaskDelay(100 / portTICK_RATE_MS);

  uart0_putc('D');  /* debug: about to wr_str hello */
  wr_str_LCD((INT8U*)"hello");
  vTaskDelay(500 / portTICK_RATE_MS);

  uart0_putc('E');  /* debug: about to move(0,1) */
  move_LCD(0,1);
  vTaskDelay(100 / portTICK_RATE_MS);

  uart0_putc('F');  /* debug: about to wr_str world */
  wr_str_LCD((INT8U*)"world");
  vTaskDelay(500 / portTICK_RATE_MS);

  uart0_putc('G');  /* debug: done, looping */

  /* Loop forever */
  while(1)
  {
    vTaskDelay(5000 / portTICK_RATE_MS);
  }
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
  enum LCD_states state = LCD_POWER_UP;

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
          /* debug: report init command */
          uart0_putc('I');
          uart_puthex(LCD_init_sequense[LCD_init_idx]);
          uart0_putc('\n');
          wr_ctrl_LCD( LCD_init_sequense[LCD_init_idx++] );
        }
        else
        {
          state = LCD_READY;
          uart0_putc('R'); /* debug: LCD_READY reached */
        }
        vTaskDelay( 100 / portTICK_RATE_MS );
        break;

      case LCD_READY:
        if( xQueueReceive( lcd_queue, &ch, portMAX_DELAY ) == pdTRUE )
        {
          switch( ch )
          {
            case 0xFF:
              uart0_putc('C'); uart_puthex(0xFF); uart0_putc('\n');
              clr_LCD();
              break;
            case ESC:
              uart0_putc('S'); uart_puthex(ch); uart0_putc('\n');
              state = LCD_ESC_RECEIVED;
              break;
            default:
              uart0_putc('W'); uart_puthex(ch); uart0_putc('\n');
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
