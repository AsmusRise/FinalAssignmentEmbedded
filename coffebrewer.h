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



/*****************************   Constants   *******************************/

/*****************************   Functions   *******************************/

void coffebrewer_task(void *pvParameters);
/****************************** End Of Module *******************************/

#endif /*COFFEBREWER_H*/