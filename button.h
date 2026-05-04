/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: button.h
*
* PROJECT....: Traffic Light
*
* DESCRIPTION: Button states and functions
*****************************************************************************/
#ifndef BUTTON_H
#define BUTTON_H

/***************************** Include files *******************************/

/*****************************    Defines    *******************************/
// Button states
#define IDLE 0
#define FIRST_PRESS 1
#define SECOND_PRESS 2
#define LONG_PRESS 3
#define FIRST_RELEASE 4
#define SINGLE_PRESS 5
#define DOUBLE_PRESS 6

#define BS_IDLE           0
#define BS_FIRST_PUSH     1
#define BS_FIRST_RELEASE  2
#define BS_SECOND_PUSH    3
#define BS_LONG_PUSH      4

/*****************************   Constants   *******************************/

/*****************************   Functions   *******************************/
INT8U select_button(void);
INT8U button_pushed_1(void);
INT8U button_pushed_2(void);

void button_1_Task(void *pvParameters);
void button_2_Task(void *pvParameters);
/****************************** End Of Module *******************************/

#endif /*BUTTON_H*/
