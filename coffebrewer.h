/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: coffebrewer.h
*
* PROJECT....:  Final Assignment
*
* DESCRIPTION: Brew logic handler
*****************************************************************************/
#ifndef COFFEBREWER_H
#define COFFEBREWER_H

/***************************** Include files *******************************/

/*****************************    Defines    *******************************/
 //brewer states
#define PRODUCT_SELECT 0
#define PAYMENT_SELECT 1
#define CARD 2
#define PINCODE 3
#define CASH 4
#define CUP_PRESENCE 5
#define ESPRESSO_BREWING 6
#define LATTE_BREWING 7
#define FILTER_COFFEE 8
#define TAKE_CUP 9

//product selection defines
#define NO_SELECTION 0
#define ESPRESSO 6
#define LATTE 7
#define FILTER_COFFEE 8

//timer defines
#define TIMER1 1
#define TIMER2 2
#define TIMER3 3
#define LED_BLINK 25 //25*10 ms = 250 ms


#define LEDON 1
#define LEDOFF 0

#define CASH 1
#define CARD 2



/*****************************   Constants   *******************************/

/*****************************   Functions   *******************************/

void coffebrewer_task(void *pvParameters);
/****************************** End Of Module *******************************/

#endif /*COFFEBREWER_H*/