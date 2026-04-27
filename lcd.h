/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: lcd.h
*
* PROJECT....: Coffee Machine
*
* DESCRIPTION: HD44780 16x2 LCD driver (4-bit mode) for EMP board.
*              Adapted from Assignment 5 solution to use FreeRTOS.
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

#ifndef _LCD_H
  #define _LCD_H

/***************************** Include files *******************************/
#include "emp_type.h"

/*****************************    Defines    *******************************/
// Special ASCII characters
#define LF      0x0A
#define FF      0x0C
#define CR      0x0D
#define ESC     0x1B

/*****************************   Constants   *******************************/

/*****************************   Functions   *******************************/

INT8U wr_ch_LCD( INT8U Ch );
/*****************************************************************************
*   Input    : Character to write
*   Output   : Result of queue send
*   Function : Queue a character for the LCD task to process.
*              OBSERVE: LCD task needs ~20ms to print one character.
*****************************************************************************/

void wr_str_LCD( INT8U *pStr );
/*****************************************************************************
*   Input    : Pointer to null-terminated string
*   Output   : -
*   Function : Queue a string for the LCD task to process.
*****************************************************************************/

void move_LCD( INT8U x, INT8U y );
/*****************************************************************************
*   Input    : x - column (0-15), y - row (0-1)
*   Output   : -
*   Function : Move cursor to position (x,y) via ESC sequence.
*****************************************************************************/

void lcd_task( void *pvParameters );
/*****************************************************************************
*   Input    : pvParameters (unused)
*   Output   : -
*   Function : FreeRTOS task - handles LCD init and character output.
*****************************************************************************/

/****************************** End Of Module *******************************/
#endif
