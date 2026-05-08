#include "coffebrewer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "emp_type.h"
#include "color_led.h"
#include "key.h"
#include "lcd.h"
#include "uart0.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

extern QueueHandle_t button_queue1;
extern QueueHandle_t button_queue2;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;
extern QueueHandle_t key_queue;
extern QueueHandle_t lcd_queue;
extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t encoder_queue;
extern SemaphoreHandle_t timer1Semaphore;
extern SemaphoreHandle_t timer2Semaphore;
extern SemaphoreHandle_t timer3Semaphore;

void waitForTimer(INT8U timerID);
void displayUpdate(const char *line1, const char *line2);
BOOLEAN selectConfirm(void);

INT16U brewerState = PRODUCT_SELECT;
INT16U espresso_price_dkk = DEFAULT_ESPRESSO_PRICE;
INT16U latte_price_dkk = DEFAULT_LATTE_PRICE;
INT16U filter_price_per_cl_dkk = DEFAULT_FILTER_COFFEE_PRICE;

static INT32U time_of_day_base_seconds = 0;
static TickType_t time_of_day_base_tick = 0;

INT16U selectedProduct = 0;
volatile INT16U timer1 = 0;
volatile INT16U timer2 = 0;
volatile INT16U timer3 = 0;
INT8U key_buffer = 0;
INT8U keyCounter = 0;
INT8U keylist[20];
INT64U cardNumber = 0;
INT16U sum = 0;
INT16U pincode = 0;
INT16U cashInserted = 0;
INT16U paymentType = 0; //2 for card, 1 for cash

FP32 cardSumToPay = 0.0f;
FP32 coffeeDispensed = 0.0f;
FP32 coffeeRate = 0.6f;
FP32 remaining_cash = 0.0f;
FP32 perTickAmount = 0.0f;

char line1[17];
char line2[17];

INT32U brewer_get_time_of_day_seconds(void)
{
    TickType_t now_tick = xTaskGetTickCount();
    TickType_t elapsed_ticks = now_tick - time_of_day_base_tick;
    INT32U elapsed_seconds = ((INT32U)elapsed_ticks * (INT32U)portTICK_RATE_MS) / 1000U;
    INT32U seconds_of_day = (time_of_day_base_seconds + elapsed_seconds) % 86400U;
    return seconds_of_day;
}

void brewer_get_time_of_day_hms(INT8U *hours, INT8U *minutes, INT8U *seconds)
{
    INT32U tod = brewer_get_time_of_day_seconds();

    if(hours != NULL)
    {
        *hours = (INT8U)(tod / 3600U);
    }
    if( minutes != NULL)
    {
        *minutes = (INT8U)((tod % 3600U) / 60U);
    }
    if(seconds != NULL)
    {
        *seconds = (INT8U)(tod % 60U);
    }
}

static void startTimer(INT8U timerID, INT16U ticks)
{
    timer_command_t command = {timerID, ticks};

    switch(timerID)
    {
        case TIMER1:
            xSemaphoreTake(timer1Semaphore, 0);
            break;
        case TIMER2:
            xSemaphoreTake(timer2Semaphore, 0);
            break;
        case TIMER3:
            xSemaphoreTake(timer3Semaphore, 0);
            break;
        default:
            break;
    }

    xQueueSend(timerCommandQueue, &command, portMAX_DELAY);
    taskYIELD();
}


void give_change(){
    while(cashInserted > 0) //one coin at a time by flashing green led
    {
        startTimer(TIMER1, LED_BLINK);
        waitForTimer(TIMER1);
        xQueueSend(greenQueue, &(INT16U){LEDON}, portMAX_DELAY);
        startTimer(TIMER1, LED_BLINK);
        waitForTimer(TIMER1);
        xQueueSend(greenQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
        cashInserted -= 1;
        //update display with remaining change
        snprintf(line1, sizeof(line1), "Change: $%.2f", (float)cashInserted / 100.0f);
        line2[0] = '\0';
        displayUpdate(line1, line2);
    }
}

void timer_task(void *pvParameters) //needs semaphores... (everywhere)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    displayUpdate(" ", " ");
    while(1)
    {
        timer_command_t command;
        while(xQueueReceive(timerCommandQueue, &command, 0) == pdTRUE)
        {
            switch(command.timer_id)
            {
                case TIMER1:
                    timer1 = command.ticks;
                    break;
                case TIMER2:
                    timer2 = command.ticks;
                    break;
                case TIMER3:
                    timer3 = command.ticks;
                    break;
                default:
                    break;
            }
        }

        if(timer1 > 0)
        {
            timer1--;
            if(timer1 == 0) xSemaphoreGive(timer1Semaphore);

        }
        if(timer2 > 0)
        {
            timer2--;
            if(timer2 == 0) xSemaphoreGive(timer2Semaphore);
        }
        if(timer3 > 0)
        {
            timer3--;
            if(timer3 == 0) xSemaphoreGive(timer3Semaphore);
        }
        vTaskDelayUntil(&xLastWakeTime, 10 / portTICK_RATE_MS);
    }
}

void waitForTimer(INT8U timerID)
{
    switch(timerID)
    {
        case 1:
            xSemaphoreTake(timer1Semaphore, portMAX_DELAY);
            break;
        case 2:
            xSemaphoreTake(timer2Semaphore, portMAX_DELAY);
            break;
        case 3:
            xSemaphoreTake(timer3Semaphore, portMAX_DELAY);
    }
}

//function to print to display
void displayUpdate(const char *line1, const char *line2)
{
    char buf1[17];
    char buf2[17];
    static char prev1[17] = "";
    static char prev2[17] = "";

    if(line1 == NULL) line1 = "";
    if(line2 == NULL) line2 = "";

    /* copy and ensure NUL-termination, limit to 16 chars for 16x2 LCD */
    strncpy(buf1, (char *)line1, 16);
    buf1[16] = '\0';
    strncpy(buf2, (char *)line2, 16);
    buf2[16] = '\0';

    /* if nothing changed, don't rewrite the LCD */
    if (strcmp(prev1, buf1) == 0 && strcmp(prev2, buf2) == 0) {
        return;
    }

    /* snapshot what we'll show (so future calls can compare) */
    strncpy(prev1, buf1, 17);
    strncpy(prev2, buf2, 17);


    clr_LCD();
    move_LCD(0,0);
    wr_str_LCD((INT8U*)buf1);
    move_LCD(0,1);
    wr_str_LCD((INT8U*)buf2);
}

BOOLEAN selectConfirm(void)
{
    while(xQueueReceive(key_queue, &key_buffer, portMAX_DELAY) == pdTRUE)
    {
        if(key_buffer == '#')
        {
            return 1;
        } else if(key_buffer == '*')
        {
            return 0;
        }
    }

    return 0;
}

void coffebrewer_task(void *pvParameters)
{

    INT16U i;
    static BOOLEAN transaction_logged = 0;

    (void)pvParameters;
    while(1)
    {
        switch (brewerState)
        {
        case INITIAL_UART_STATE:
        {
            static BOOLEAN startup_config_done = 0;

            displayUpdate("Coffee Brewer", "UART setup mode");

            if(startup_config_done == 0)
            {
                time_of_day_base_seconds = 0;
                /* debug: before UART startup config */
                uart0_putc('p');
                uart0_read_startup_config(&espresso_price_dkk,
                                          &latte_price_dkk,
                                          &filter_price_per_cl_dkk,
                                          &time_of_day_base_seconds);
                /* debug: after UART startup config */
                uart0_putc('q');

                time_of_day_base_tick = xTaskGetTickCount();

                snprintf(line1, sizeof(line1), "E:%u L:%u F:%u",
                         (unsigned)espresso_price_dkk,
                         (unsigned)latte_price_dkk,
                         (unsigned)filter_price_per_cl_dkk);
                INT32U tod = brewer_get_time_of_day_seconds();
                snprintf(line2, sizeof(line2), "T:%02u:%02u:%02u",
                         (unsigned)(tod / 3600U),
                         (unsigned)((tod % 3600U) / 60U),
                         (unsigned)(tod % 60U));
                displayUpdate(line1, line2);
                startup_config_done = 1;
                brewerState = PRODUCT_SELECT;
            }
            break;
        }
        case PRODUCT_SELECT:
            leds_off();
            //Reset all variables to default
            xQueueReset(key_queue);
            selectedProduct = NO_SELECTION;
            paymentType = NO_SELECTION;
            cardNumber = 0;
            sum = 0;
            cashInserted = 0;
            for(i = 0; i < keyCounter; i++)
            {
                keylist[i] = 0;
            }
            keyCounter = 0;
            displayUpdate("Select Product:", "1:Esp 2:Lat 3:FC");

            /* debug: about to wait for product select */
            uart0_putc('A');
            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {

                //button press received, state is now set to the button that was pressed
                switch (key_buffer)
                {
                case '1':
                    //update display
                    displayUpdate("Select: Espresso", "#: Yes, *: No");

                    if(selectConfirm())
                    {
                        selectedProduct = ESPRESSO;
                        brewerState = PAYMENT_SELECT;
                    }
                    break;
                case '2':
                    //update display
                    displayUpdate("Select: Latte", "#: Yes, *: No");


                    if(selectConfirm())
                    {
                        selectedProduct = LATTE;
                        brewerState = PAYMENT_SELECT;
                    }
                    break;
                case '3':
                    //update display
                    displayUpdate("Select: FilCof", "#: Yes, *: No");


                    if(selectConfirm())
                    {
                        selectedProduct = FILTER_COFFEE;
                        brewerState = PAYMENT_SELECT;
                    }
                    break;
                default:
                    // not valid product, ignore
                    break;
                }
            }
            /* debug: leaving PRODUCT_SELECT */
            uart0_putc('a');
            break;
        case PAYMENT_SELECT:
            //update display
            xQueueReset(key_queue);
            displayUpdate("Select Payment:", "1:Card 2:Cash");

            /* debug: about to wait for payment select */
            uart0_putc('B');
            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {

                //button press received, state is now set to the button that was pressed
                switch (key_buffer)
                {
                case '1':
                   //update display
                    displayUpdate("Selected: Card", "#: Yes, *: No");
                    if(selectConfirm())
                    {
                        brewerState = CARD_ENTRY;
                        paymentType = PAY_CARD;
                    }
                    break;
                case '2':
                    //update display
                    displayUpdate("Selected: Cash", "#: Yes, *: No");
                    if(selectConfirm())
                    {
                        brewerState = CASH_ENTRY;
                        paymentType = PAY_CASH;
                    }
                    break;
                default:
                    // not valid product, ignore
                    break;
                }
            }
            /* debug: leaving PAYMENT_SELECT */
            uart0_putc('b');
            break;
        case CARD_ENTRY:
            //update display
            xQueueReset(key_queue);
            displayUpdate("Enter", "Card Number:");

            /* debug: about to wait for card entry */
            uart0_putc('C');
            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
                switch (key_buffer)
                {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': // card number to queue update display accordingly
                    keylist[keyCounter] = key_buffer;
                    keyCounter++;
                    if(keyCounter >= 16)
                    {
                        // max card number length reached, confirm selection
                        //update display
                        displayUpdate("'#' to confirm", "");

                        if(selectConfirm())
                        {

                            //convert to number and save for pin confirmation
                            for(i = 0; i < keyCounter; i++)
                            {
                                cardNumber = cardNumber * 10 + (keylist[i] - '0');
                            }
                            //clear keylist and reset counter
                            for(i = 0; i < keyCounter; i++)
                            {
                                keylist[i] = 0;
                            }
                            keyCounter = 0;
                            brewerState = PINCODE;
                        } else {
                            //clear keylist and reset counter
                            for(i = 0; i < keyCounter; i++)
                            {
                                keylist[i] = 0;
                            }
                            keyCounter = 0;
                            //back to card number input, update display
                            displayUpdate("Enter", "Card Number:");
                        }

                    }
                    //update display

                    break;
                case '*': //backspace
                    if(keyCounter > 0)
                    {
                        keyCounter--;
                        keylist[keyCounter] = 0;
                    }
                    //update display
                    break;
                case '#': //clear
                    for(i = 0; i < keyCounter; i++)
                    {
                        keylist[i] = 0;
                    }
                    keyCounter = 0;
                    //update display
                    break;
                default:
                    break;
                }
            }
            /* debug: leaving CARD_ENTRY */
            uart0_putc('c');
            break;
        case PINCODE:
            //update display
            xQueueReset(key_queue);
                displayUpdate("Enter PIN Code:", "#: Done, *: Clr");


            /* debug: about to wait for pincode */
            uart0_putc('D');
            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
                switch (key_buffer)
                {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': // pincode to queue update display accordingly
                    keylist[keyCounter] = key_buffer;
                    keyCounter++;
                    if(keyCounter >= 4)
                    {
                        //max pincode length reached run logic to check if correct....
                        //accept code if both are odd or both are even else reject.
                        sum = keylist[0] + cardNumber % 10;
                        if(sum % 2 == 0)
                        {
                            //update display with success
                            brewerState = CUP_PRESENCE;//move to next state (we just assume infinte money for filter coffee so no change or anything needed)
                        } else {
                            //update display with failure and return to product selection
                            brewerState = PRODUCT_SELECT;
                        }
                    }
                    //update display with * for each number entered for security
                    break;
                case '*': //backspace
                    if(keyCounter > 0)
                    {
                        keyCounter--;
                        keylist[keyCounter] = 0;
                    }
                    //update display
                    break;
                case '#': //clear
                    for(i = 0; i < keyCounter; i++)
                    {
                        keylist[i] = 0;
                    }
                    keyCounter = 0;
                    //update display
                    break;
                default:

                    break;
                }
            }
            /* debug: leaving PINCODE */
            uart0_putc('d');
            break;
        case CASH_ENTRY:
            //update display
            displayUpdate("Insert Cash", "");
            xQueueReset(key_queue);
            xQueueReset(encoder_queue);
            //get input from encoder
                if(xQueueReceive(encoder_queue, &key_buffer, 20) == pdTRUE) //dont wait indef cause be also need to keep track of confirm/cancel input from keypad
                {
                    uart0_putc('K');
                    leds_on();
                    //update display with current sum
                    //snprintf(line1, sizeof(line1), "%u", (unsigned)cashInserted); //something is wrong witht the snprintf
                    snprintf(line1, sizeof(line1), "%u", 15u); //just for testing
                    uart0_putc('k');
                    displayUpdate("Current cash:", line1);
                    if(key_buffer == 1)
                    {
                        cashInserted += 20;
                    } else if(key_buffer == 2)
                    {
                      cashInserted += 5;
                    }
                }
                if(xQueueReceive(key_queue,  &key_buffer, 20) == pdTRUE)
                {
                    if(key_buffer == '#') //we are done with payment
                    {
                        
                        switch (selectedProduct)
                        {
                        case ESPRESSO:
                            if (cashInserted >= espresso_price_dkk)
                            {
                                //payment successful, update display and move to brewing state
                                cashInserted -= espresso_price_dkk; //calculate change to be given
                                give_change();
                                brewerState = CUP_PRESENCE;
                            } else {
                                //not enough cash inserted, update display accordingly
                                //maybe we let them continue to insert cash instead of going back to product selection?
                            }
                            break;
                        case LATTE:
                            if (cashInserted >= latte_price_dkk)
                            {
                                //payment successful, update display and move to brewing state
                                cashInserted -= latte_price_dkk; //calculate change to be given
                                give_change();
                                brewerState =  CUP_PRESENCE;
                            } else {
                                //not enough cash inserted, update display accordingly
                                //maybe we let them continue to insert cash instead of going back to product selection?
                            }
                            break;
                        case FILTER_COFFEE: //depends on amount dispenced so handled later just continue for now.
                            brewerState = CUP_PRESENCE;
                            paymentType = 1;
                            break;
                        default:
                            break;
                        }
                    } else if(key_buffer == '*') //cancel payment and return change
                    {
                        give_change();
                        brewerState = PRODUCT_SELECT;
                        //update display
                        displayUpdate("Payment cancel.", "Giving change...");
                    }
                }
            break;
        case CUP_PRESENCE:
            //update display
            xQueueReset(button_queue1);
            /* debug: waiting for cup presence */
            uart0_putc('E');
            //wait for signal from "cup sensor" (aka button input)
            if(xQueueReceive(button_queue1, &key_buffer, portMAX_DELAY) == pdTRUE){ //just check if its been clicked
                brewerState = READY_TO_BREW;
            }
            /* debug: leaving CUP_PRESENCE */
            uart0_putc('e');
            break;
        case READY_TO_BREW:
            //update display (waiting for start input)
            xQueueReset(button_queue2);
            /* debug: waiting for ready to brew trigger */
            uart0_putc('F');
            if(xQueueReceive(button_queue2,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
                brewerState = selectedProduct; //move to brewing state based on selected product
            }
            /* debug: leaving READY_TO_BREW */
            uart0_putc('f');
            break;
        case ESPRESSO_BREWING:
            //update display with brewing status
            displayUpdate("Espresso select", "Grinding beans...");

            //Grind coffee for 7.5 s, indicated by the yellow LED, then brew coffee for 14 s, indicated by red LED
            startTimer(TIMER1, GRIND_TIME); //7.5 seconds
            while (timer1 >0 )
            {
                startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                if(timer1 > GRIND_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                    xQueueSend(yellowQueue, &(INT16U){LEDON}, portMAX_DELAY);
                 }
                startTimer(TIMER2, LED_BLINK);
                 if(timer1 > GRIND_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {                    
                    waitForTimer(TIMER2);
                 }
                xQueueSend(yellowQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
            }
            startTimer(TIMER1, BREW_TIME); //14 seconds
            displayUpdate("Espresso select", "Brewing coffee..");
            while (timer1 >0 )
            {
                startTimer(TIMER2, LED_BLINK); //blink red led while brewing
                if(timer1 > BREW_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                     xQueueSend(redQueue, &(INT16U){LEDON}, portMAX_DELAY);
                 }
                startTimer(TIMER2, LED_BLINK);
                 if(timer1 > BREW_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                 }
                xQueueSend(redQueue, &(INT16U){LEDOFF}, portMAX_DELAY); //always turn off red led after brew time even if we stopped blinking before
            }

            //update display with brew complete
            displayUpdate("Espresso ready!", "Please take cup");

            //clear take cup queue here so you can take it instantly after brew complete if you want to
            startTimer(TIMER1, BREW_COMPLETE_TIME); //1 second to indicate brew complete before allowing cup to be taken
            waitForTimer(TIMER1);
            brewerState = TAKE_CUP; //move to next state
            break;
        case LATTE_BREWING:
            //same as espresso but with an extra step of frothing milk for 6.2 seconds with the green led on after grinding and brewing
            startTimer(TIMER1, GRIND_TIME); //7.5 seconds
            displayUpdate("Latte selected", "Grinding beans...");
            while (timer1 >0 )
            {
                startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                if(timer1 > GRIND_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                     xQueueSend(yellowQueue, &(INT16U){LEDON}, portMAX_DELAY);
                 }
                startTimer(TIMER2, LED_BLINK);
                 if(timer1 > GRIND_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {                    waitForTimer(TIMER2);
                 }
                xQueueSend(yellowQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
            }
            displayUpdate("Latte selected", "Brewing coffee...");

            startTimer(TIMER1, BREW_TIME); //14 seconds
            while (timer1 >0 )
            {
                startTimer(TIMER2, LED_BLINK); //blink red led while brewing
                if(timer1 > BREW_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                     xQueueSend(redQueue, &(INT16U){LEDON}, portMAX_DELAY);
                 }
                startTimer(TIMER2, LED_BLINK);
                 if(timer1 > BREW_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                 }
                xQueueSend(redQueue, &(INT16U){LEDOFF}, portMAX_DELAY); //always turn off red led after brew time even if we stopped blinking before
            }
            startTimer(TIMER1, LATTE_FROTH_TIME); //6.2 seconds
            displayUpdate("Latte selected", "Frothing milk...");
            while (timer1 >0 )
            {
                startTimer(TIMER2, LED_BLINK); //blink green led while frothing
                if(timer1 > LATTE_FROTH_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                     xQueueSend(greenQueue, &(INT16U){LEDON}, portMAX_DELAY);
                 }
                startTimer(TIMER2, LED_BLINK);
                 if(timer1 > LATTE_FROTH_TIME - LED_BLINK) //only continue blinking if we have enough time left
                {
                    waitForTimer(TIMER2);
                 }
                xQueueSend(greenQueue, &(INT16U){LEDOFF}, portMAX_DELAY); //always turn off green led after froth time even if we stopped blinking before
            }
            //update display with brew complete
            displayUpdate("Latte ready!", "Please take cup");

            //clear take cup queue here so you can take it instantly after brew complete if you want to
            startTimer(TIMER1, BREW_COMPLETE_TIME); //1 second to indicate brew complete before allowing cup to be taken
            waitForTimer(TIMER1);
            brewerState = TAKE_CUP; //move to next state
            break;
        case FILTER_COFFEE_BREWING:
            //completely different...
            startTimer(TIMER1, SLOW_RATE_TIME);
            cardSumToPay = 0.0f;
            remaining_cash = cashInserted; 
            /*– Dispensed while start (button 2) is pressed or until prepaid amount is reached.
            – Rate: starts slow at 0.6 cl/s for 3 s, then 1.45 cl/s. More coffee can be added by repeated
            pushes of the start button (button 2), but after 5 seconds of inactivity, the coffee dispensing
            is finished. The brewing process is indicated by yellow LED.
            – Display shows amount, unit price, total price */
            //takes 20 ms pr input from button 2 when being held down so we should eat at a matching rate.
            if(paymentType == PAY_CASH)
            {
                
                
                coffeeRate = 0.6f; //start rate at 0.6 cl/s
                perTickAmount = coffeeRate * 0.01f; //calculate how much we should dispense every 10 ms to match the desired rate in cl/s
                while(timer1 > 0)
                {
                    displayUpdate("FC Brewing", " Dispensing...");
                    if(remaining_cash <= 0.0f) //if we have dispensed all the coffee the customer paid for we can just end the brewing process even if they havent released the button yet.
                    {
                        displayUpdate("Ran out of money", "Please take cup!");
                        brewerState = TAKE_CUP;
                        break;
                    }
                    else if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE) //check if button is being held down
                    {

                        if(key_buffer == 1)
                        {
                            startTimer(TIMER2, INACTIVITY_TIME);
                            coffeeDispensed += perTickAmount; 
                            remaining_cash -= perTickAmount * filter_price_per_cl_dkk;
                            snprintf(line1, sizeof(line1), "Amt: %3.0f cl U: %1u", (double)coffeeDispensed, (unsigned)filter_price_per_cl_dkk);
                            snprintf(line2, sizeof(line2), "Total: $%4.1f", coffeeDispensed * filter_price_per_cl_dkk);
                            displayUpdate(line1, line2);
                        }
                    }
                    startTimer(TIMER3, 1);
                    waitForTimer(TIMER3);
                }

                coffeeRate = 1.45f; //after 3 seconds we increase the rate to 1.45 cl/s
                perTickAmount = coffeeRate * 0.01f; //calculate how much we should dispense every 10 ms to match the desired rate in cl/s

                while (remaining_cash > 0.0f && timer2 > 0) //keep dispensing as long as we have coffee to dispense and we havent had 5 seconds of inactivity
                {
                    if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE)
                    {
                        if(key_buffer == 1)
                        {
                            startTimer(TIMER2, INACTIVITY_TIME);
                            
                            coffeeDispensed += perTickAmount; 
                            remaining_cash -= perTickAmount * filter_price_per_cl_dkk;
                            snprintf(line1, sizeof(line1), "Amt: %3.0f cl U: %1u", (double)coffeeDispensed, (unsigned)filter_price_per_cl_dkk);
                            snprintf(line2, sizeof(line2), "Total: $%4.1f", coffeeDispensed * filter_price_per_cl_dkk);
                            displayUpdate(line1, line2);
                        }
                    }
                    startTimer(TIMER3, 1);
                    waitForTimer(TIMER3);
                }
            }
            else if(paymentType == PAY_CARD)
            {
                coffeeRate = 0.6f; //start rate at 0.6 cl/s
                perTickAmount = coffeeRate * 0.01f; //calculate how much we should dispense every 10 ms to match the desired rate in cl/s
                while(timer1 > 0)
                    {
                    //for card payment we just let them dispense until they release the button since we cant really track the amount they need to pay in the same way as with cash and we dont want to just set a fixed amount that they can dispense since it might not be enough or it might be too much.
                    if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE) //check if button is being held down
                        {
                            if(key_buffer == 1)
                            {
                                startTimer(TIMER2, INACTIVITY_TIME);
                                coffeeDispensed += perTickAmount;
                                cardSumToPay += perTickAmount * filter_price_per_cl_dkk; //update the sum to pay based on how much coffee they have dispensed
                                snprintf(line1, sizeof(line1), "Amt: %3.0f cl U: %1u", (double)coffeeDispensed, (unsigned)filter_price_per_cl_dkk);
                                snprintf(line2, sizeof(line2), "Total: $%4.1f", cardSumToPay);
                                displayUpdate(line1, line2);
                            }
                        }
                    startTimer(TIMER3, 1);
                    waitForTimer(TIMER3); //just to make sure we have a small delay before we start checking for inactivity so that we dont end the brewing process immediately if they just click the button once instead of holding it down
                    }

                coffeeRate = 1.45f; //after 3 seconds we increase the rate to 1.45 cl/s
                perTickAmount = coffeeRate * 0.01f; //calculate how much we should dispense every 10 ms to match the desired rate in cl/s
                while(timer2 > 0)
                    {
                    if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE)
                        {
                        if(key_buffer == 1)
                        {
                            startTimer(TIMER2, INACTIVITY_TIME);
                            
                            coffeeDispensed += perTickAmount; 
                            cardSumToPay += perTickAmount * filter_price_per_cl_dkk; //update the sum to pay based on how much coffee they have dispensed
                            snprintf(line1, sizeof(line1), "Amt: %3.0f cl U: %1u", (double)coffeeDispensed, (unsigned)filter_price_per_cl_dkk);
                            snprintf(line2, sizeof(line2), "Total: $%4.1f", cardSumToPay);
                            displayUpdate(line1, line2);
                        }
                        }
                    startTimer(TIMER3, 1);
                    waitForTimer(TIMER3);
                        
                    } 
            }
            brewerState = TAKE_CUP; 
            
            break;
        case TAKE_CUP:
            //update display to take cup
            displayUpdate("Please take cup", "press 1");
            xQueueReset(button_queue1);
            
            //Log the completed transaction
            if(transaction_logged == 0)
            {
                transaction_t transaction;

                transaction.uptime_sec = brewer_get_time_of_day_seconds();
                transaction.product = (INT8U)selectedProduct;

                if(selectedProduct == ESPRESSO)
                {
                    transaction.price = espresso_price_dkk;
                    transaction.amount = 1;
                }
                else if(selectedProduct == LATTE)
                {
                    transaction.price = latte_price_dkk;
                    transaction.amount = 1;
                }
                else if(selectedProduct == FILTER_COFFEE)
                {
                    transaction.price = (INT16U)((paymentType == PAY_CASH ? (coffeeDispensed * filter_price_per_cl_dkk) : cardSumToPay) + 0.5f);
                    transaction.amount = (INT8U)(coffeeDispensed + 0.5f);
                }

                if(paymentType == PAY_CASH)
                {
                    strcpy(transaction.payment, "CASH");
                }
                else if(paymentType == PAY_CARD)
                {
                    snprintf(transaction.payment, sizeof(transaction.payment), "%016llu", cardNumber);
                }

                xQueueOverwrite(transaction_queue, &transaction);
                transaction_logged = 1;
            }
            
            //wait for signal from "cup sensor" (aka button input) that cup has been taken
            /* debug: waiting for take cup */
            uart0_putc('G');
            if(xQueueReceive(button_queue1, &key_buffer, portMAX_DELAY) == pdTRUE){ //just check if its been clicked
                brewerState = PRODUCT_SELECT; //back to start for next customer
                transaction_logged = 0;
            }
            /* debug: leaving TAKE_CUP */
            uart0_putc('g');
            break;

        default:
            //error code, should never happen
            break;
        }
    }
}
