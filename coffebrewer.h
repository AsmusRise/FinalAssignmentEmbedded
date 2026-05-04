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
#define CARD_ENTRY 2
#define PINCODE 3
#define CASH_ENTRY 4
#define CUP_PRESENCE 5
#define READY_TO_BREW 6
#define ESPRESSO_BREWING 7
#define LATTE_BREWING 8
#define FILTER_COFFEE_BREWING 9
#define TAKE_CUP 10

//product selection defines
#define NO_SELECTION 0
#define ESPRESSO 7 //same values as for the general state machine so we can just set brewerState to the selectedProduct.
#define LATTE 8
#define FILTER_COFFEE 9

//payment type defines
#define PAY_CASH 1
#define PAY_CARD 2

//timer defines
#define TIMER1 1
#define TIMER2 2
#define TIMER3 3
#define LED_BLINK 25 //25*10 ms = 250 ms
#define GRIND_TIME 750 //7.5 seconds
#define BREW_TIME 1400 //14 seconds
#define LATTE_FROTH_TIME 620 //6.2 seconds
#define BREW_COMPLETE_TIME 1000 //1 second


#define LEDON 1
#define LEDOFF 0

#define ESPRESSO_PRICE 15 //dkk
#define LATTE_PRICE 27 //dkk
#define FILTER_COFFEE_PRICE 3 //dkk pr cl



/*****************************   Constants   *******************************/

/*****************************   Functions   *******************************/

void coffebrewer_task(void *pvParameters);
void timer_task(void *pvParameters);
/****************************** End Of Module *******************************/

#endif /*COFFEBREWER_H*/
