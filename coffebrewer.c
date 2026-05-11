#include "coffebrewer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "emp_type.h"
#include "color_led.h"
#include "key.h"
#include "lcd.h"
#include "formatter.h"
#include "uart0.h"
#include "logger.h"
#include <string.h>

extern QueueHandle_t button_queue1;
extern QueueHandle_t button_queue2;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;
extern QueueHandle_t key_queue;
extern QueueHandle_t lcd_queue;
extern QueueHandle_t formatter_request_queue;
extern QueueHandle_t formatter_reply_queue;
extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t encoder_queue;
extern SemaphoreHandle_t timer1Semaphore;
extern SemaphoreHandle_t timer2Semaphore;
extern SemaphoreHandle_t timer3Semaphore;

void waitForTimer(INT8U timerID);
void displayUpdate(const char *line1, const char *line2);
BOOLEAN selectConfirm(void);

INT16U brewerState = INITIAL_UART_STATE;
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
    BOOLEAN ok;
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
        ok = formatter_format_change(cashInserted, line1, line2);
        if(ok != pdTRUE)
        {
            line1[0] = '\0';
            line2[0] = '\0';
        }
        displayUpdate(line1, line2);
    }
    xQueueSend(greenQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
}

void timer_task(void *pvParameters) //needs semaphores... (everywhere)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
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
    static char buf1[17];
    static char buf2[17];
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

    /* Send clear command through queue (0xFF) instead of calling directly */
    wr_ch_LCD(0xFF);
    
    /* Send move to (0,0) via queue */
    move_LCD(0,0);
    wr_str_LCD((INT8U*)buf1);
    
    /* Send move to (0,1) via queue */
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
    int ret;
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
                uart0_read_startup_config(&espresso_price_dkk,
                                          &latte_price_dkk,
                                          &filter_price_per_cl_dkk,
                                          &time_of_day_base_seconds);

                time_of_day_base_tick = xTaskGetTickCount();
                formatter_format_startup(espresso_price_dkk,
                                     latte_price_dkk,
                                     filter_price_per_cl_dkk,
                                     brewer_get_time_of_day_seconds(),
                                     line1,
                                     line2);
                displayUpdate(line1, line2);
                startup_config_done = 1;
                brewerState = PRODUCT_SELECT;
            }
            break;
        }
        case PRODUCT_SELECT:
            //leds_off(); // doesnt work
            //Reset all variables to default
            xQueueReset(key_queue);
            xQueueReset(button_queue1);
            xQueueReset(button_queue2);
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

            break;
        case PAYMENT_SELECT:
            //update display
            xQueueReset(key_queue);
            displayUpdate("Select Payment:", "1:Card 2:Cash");

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

            break;
        case CARD_ENTRY:
            //update display
            xQueueReset(key_queue);
            displayUpdate("Enter", "Card Number:");

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

            break;
        case PINCODE:
            //update display
            xQueueReset(key_queue);
            displayUpdate("Enter PIN Code:", "#: Done, *: Clr");


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
                        sum = keylist[3] + cardNumber % 10;
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

            break;
        case CASH_ENTRY:
        {
            static BOOLEAN cash_entry_shown = 0;
            
            //update display only once when entering this state
            if(cash_entry_shown == 0)
            {
                displayUpdate("Insert Cash", "");
                cash_entry_shown = 1;
            }
            
            //get input from encoder for payment amount
            if(xQueueReceive(encoder_queue, &key_buffer, 20) == pdTRUE)
            {
                if(key_buffer == 1)  // clockwise
                {
                    cashInserted += 20;
                }
                else if(key_buffer == 2)  // anti-clockwise
                {
                    cashInserted += 5;
                }
                formatter_format_cash_status(cashInserted, line1, line2);
                displayUpdate(line1, line2);
            }
            
            //get input from keypad for confirm/cancel
            if(xQueueReceive(key_queue,  &key_buffer, 0) == pdTRUE)
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
                            displayUpdate("Not enough!", "Insert more cash");
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
                            displayUpdate("Not enough!", "Insert more cash");
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
                    cashInserted = 0;
                    //update display
                    displayUpdate("Payment cancel.", "Giving change...");
                }
                }
            
            //reset flag when leaving CASH_ENTRY
            if(brewerState != CASH_ENTRY)
            {
                cash_entry_shown = 0;
            }
            break;
        }
        case CUP_PRESENCE:
        {
            static BOOLEAN cup_display_shown = 0;
            
            //clear queue and display only once
            if(cup_display_shown == 0)
            {
                xQueueReset(button_queue1);
                displayUpdate("Place cup", "Press Button 1");
                cup_display_shown = 1;
            }
            
            //wait for signal from "cup sensor" (aka button input)
            if(xQueueReceive(button_queue1, &key_buffer, portMAX_DELAY) == pdTRUE)
            { 
                brewerState = READY_TO_BREW;
                cup_display_shown = 0;  //reset for next time
            }
            break;
        }
        case READY_TO_BREW:
        {
            static BOOLEAN brew_display_shown = 0;
            
            //clear queue and display only once
            if(brew_display_shown == 0)
            {
                xQueueReset(button_queue2);
                displayUpdate("Ready to brew?", "Press Button 2");
                brew_display_shown = 1;
            }
            
            if(xQueueReceive(button_queue2,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
                brewerState = selectedProduct; //move to brewing state based on selected product
                brew_display_shown = 0;  //reset for next time
            }
            break;
        }
        case ESPRESSO_BREWING:
            //Grind coffee for 7.5 s, indicated by the yellow LED, then brew coffee for 14 s, indicated by red LED
            startTimer(TIMER1, GRIND_TIME); //7.5 seconds
            //update display with brewing status
            displayUpdate("Espresso select", "Grinding beans...");
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            while (timer1 > 0)
            {
                if(timer1 > (2 * LED_BLINK)) //only continue blinking if we have enough time left
                {
                    startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                    waitForTimer(TIMER2);
                    xQueueSend(yellowQueue, &(INT16U){LEDON}, portMAX_DELAY);

                    startTimer(TIMER2, LED_BLINK);
                    waitForTimer(TIMER2);
                }
                xQueueSend(yellowQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
                vTaskDelay(10 / portTICK_RATE_MS); //yield to let timer_task decrement timer1
            }

            startTimer(TIMER1, BREW_TIME); //14 seconds
            displayUpdate("Espresso select", "Brewing coffee..");
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            while (timer1 > 0)
            {
                if(timer1 > (2 * LED_BLINK)) //only continue blinking if we have enough time left
                {
                    startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                    waitForTimer(TIMER2);
                    xQueueSend(redQueue, &(INT16U){LEDON}, portMAX_DELAY);

                    startTimer(TIMER2, LED_BLINK);
                    waitForTimer(TIMER2);
                }
                xQueueSend(redQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
                vTaskDelay(10 / portTICK_RATE_MS); //yield to let timer_task decrement timer1
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
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            while (timer1 > 0)
            {
                if(timer1 > (2 * LED_BLINK)) //only continue blinking if we have enough time left
                {
                    startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                    waitForTimer(TIMER2);
                    xQueueSend(yellowQueue, &(INT16U){LEDON}, portMAX_DELAY);

                    startTimer(TIMER2, LED_BLINK);
                    waitForTimer(TIMER2);
                }
                xQueueSend(yellowQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
                vTaskDelay(10 / portTICK_RATE_MS); //yield to let timer_task decrement timer1
            }

            startTimer(TIMER1, BREW_TIME); //14 seconds
            displayUpdate("Latte selected", "Brewing coffee...");
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            while (timer1 > 0)
            {
                if(timer1 > (2 * LED_BLINK)) //only continue blinking if we have enough time left
                {
                    startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                    waitForTimer(TIMER2);
                    xQueueSend(redQueue, &(INT16U){LEDON}, portMAX_DELAY);

                    startTimer(TIMER2, LED_BLINK);
                    waitForTimer(TIMER2);
                }
                xQueueSend(redQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
                vTaskDelay(10 / portTICK_RATE_MS); //yield to let timer_task decrement timer1
            }

            startTimer(TIMER1, LATTE_FROTH_TIME); //6.2 seconds
            displayUpdate("Latte selected", "Frothing milk...");
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            while (timer1 > 0)
            {
                if(timer1 > (2 * LED_BLINK)) //only continue blinking if we have enough time left
                {
                    startTimer(TIMER2, LED_BLINK); //blink yellow led while grinding
                    waitForTimer(TIMER2);
                    xQueueSend(greenQueue, &(INT16U){LEDON}, portMAX_DELAY);

                    startTimer(TIMER2, LED_BLINK);
                    waitForTimer(TIMER2);
                }
                xQueueSend(greenQueue, &(INT16U){LEDOFF}, portMAX_DELAY);
                vTaskDelay(10 / portTICK_RATE_MS); //yield to let timer_task decrement timer1
            }
            //update display with brew complete
            displayUpdate("Latte ready!", "Please take cup");

            //clear take cup queue here so you can take it instantly after brew complete if you want to
            startTimer(TIMER1, BREW_COMPLETE_TIME); //1 second to indicate brew complete before allowing cup to be taken
            waitForTimer(TIMER1);
            brewerState = TAKE_CUP; //move to next state
            break;
        case FILTER_COFFEE_BREWING:
            uart0_puts("Entering FILTER_COFFEE");
            startTimer(TIMER1, SLOW_RATE_TIME);
            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
            cardSumToPay = 0.0f;
            remaining_cash = cashInserted;
            coffeeDispensed = 0.0f; 
            INT8U dipensedThisSecond = 0;
            /*– Dispensed while start (button 2) is pressed or until prepaid amount is reached.
            – Rate: starts slow at 0.6 cl/s for 3 s, then 1.45 cl/s. More coffee can be added by repeated
            pushes of the start button (button 2), but after 5 seconds of inactivity, the coffee dispensing
            is finished. The brewing process is indicated by yellow LED.
            – Display shows amount, unit price, total price */
            //takes 20 ms pr input from button 2 when being held down so we should eat at a matching rate.
            if(paymentType == PAY_CASH)
            {
                coffeeRate = 0.6f; //start rate at 0.6 cl/s
                displayUpdate("FC Brewing", " Dispensing...");
                while(timer1 > 0)
                {
                    if (timer2 == 0) 
                    {
                        dipensedThisSecond = 0;
                        startTimer(TIMER2, TIM_1_SEC);
                        vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
                    }

                    if(remaining_cash <= 0.0f) //if we have dispensed all the coffee the customer paid for we can just end the brewing process even if they havent released the button yet.
                    {
                        displayUpdate("Ran out of money", "Please take cup!");
                        brewerState = TAKE_CUP;
                        break;
                    }
                    else if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE && (dipensedThisSecond == 0)) //check if button is being held down
                    {
                        dipensedThisSecond++;
                        startTimer(TIMER3, INACTIVITY_TIME);
                        vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
                        coffeeDispensed += coffeeRate; 
                        remaining_cash -= coffeeRate * filter_price_per_cl_dkk;
                        formatter_format_progress((INT16U)(coffeeDispensed + 0.5f), // Plus a half to round up
                                                filter_price_per_cl_dkk,
                                                (INT32U)((coffeeDispensed * (FP32)filter_price_per_cl_dkk * 10.0f) + 0.5f), // Plus a half to round up
                                                line1,
                                                line2);
                        displayUpdate(line1, line2);
                    }

                    if (timer3 == 0) 
                    {
                        displayUpdate("Idle too long.", "Take Cup.");
                        brewerState = TAKE_CUP;
                    }
                }

                coffeeRate = 1.45f; //after 3 seconds we increase the rate to 1.45 cl/s

                while (timer3 > 0) //keep dispensing as long as we havent had 5 seconds of inactivity
                {
                    if (timer2 == 0) 
                    {
                        dipensedThisSecond = 0;
                        startTimer(TIMER2, TIM_1_SEC);
                        vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
                    }
                    if(remaining_cash <= 0.0f) //if we have dispensed all the coffee the customer paid for we can just end the brewing process even if they havent released the button yet.
                    {
                        displayUpdate("Ran out of money", "Please take cup!");
                        brewerState = TAKE_CUP;
                        break;
                    }
                    else if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE && (dipensedThisSecond == 0))
                    {
                        dipensedThisSecond++;
                        startTimer(TIMER3, INACTIVITY_TIME);
                        
                        coffeeDispensed += coffeeRate; 
                        remaining_cash -= coffeeRate * filter_price_per_cl_dkk;
                        formatter_format_progress((INT16U)(coffeeDispensed + 0.5f), // Plus a half to round up 
                                                filter_price_per_cl_dkk,
                                                (INT32U)((coffeeDispensed * (FP32)filter_price_per_cl_dkk * 10.0f) + 0.5f), // Plus a half to round up
                                                line1,
                                                line2);
                        displayUpdate(line1, line2);
                    }
                }
                if (timer3 == 0) 
                    {
                        displayUpdate("Idle too long.", "Take Cup.");
                        brewerState = TAKE_CUP;
                    }
            }
            else if(paymentType == PAY_CARD)
            {
                coffeeRate = 0.6f; //start rate at 0.6 cl/s
                displayUpdate("FC Brewing", " Dispensing...");
                while(timer1 > 0)
                    {
                        //for card payment we just let them dispense until they release the button 
                        // since we cant really track the amount they need to pay in the same way as 
                        // with cash and we dont want to just set a fixed amount that they can dispense 
                        // since it might not be enough or it might be too much.

                        if (timer2 == 0) 
                        {
                            dipensedThisSecond = 0;
                            startTimer(TIMER2, TIM_1_SEC);
                            vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
                        }

                        if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE && (dipensedThisSecond == 0)) //check if button is being held down
                            {
                                dipensedThisSecond++;
                                startTimer(TIMER3, INACTIVITY_TIME);
                                coffeeDispensed += coffeeRate;
                                cardSumToPay += coffeeRate * filter_price_per_cl_dkk; //update the sum to pay based on how much coffee they have dispensed
                                formatter_format_progress((INT16U)(coffeeDispensed + 0.5f),
                                                        filter_price_per_cl_dkk,
                                                        (INT32U)((cardSumToPay * 10.0f) + 0.5f),
                                                        line1,
                                                        line2);
                                displayUpdate(line1, line2);
                            }

                        if (timer3 == 0) 
                        {
                            displayUpdate("Idle too long.", "Take Cup.");
                            brewerState = TAKE_CUP;
                        }
                    }

                coffeeRate = 1.45f; //after 3 seconds we increase the rate to 1.45 cl/s
                while(timer3 > 0)
                {
                    if (timer2 == 0) 
                    {
                        dipensedThisSecond = 0;
                        startTimer(TIMER2, TIM_1_SEC);
                        vTaskDelay(10 / portTICK_RATE_MS); // yield to let timer set
                    }

                    if(xQueueReceive(button_queue2, &key_buffer, 20 ) == pdTRUE && (dipensedThisSecond == 0)) //check if button is being held down
                        {
                            dipensedThisSecond++;
                            startTimer(TIMER3, INACTIVITY_TIME);
                            coffeeDispensed += coffeeRate;
                            cardSumToPay += coffeeRate * filter_price_per_cl_dkk; //update the sum to pay based on how much coffee they have dispensed
                            formatter_format_progress((INT16U)(coffeeDispensed + 0.5f),
                                                    filter_price_per_cl_dkk,
                                                    (INT32U)((cardSumToPay * 10.0f) + 0.5f),
                                                    line1,
                                                    line2);
                            displayUpdate(line1, line2);
                        }
                    
                }

                if (timer3 == 0) 
                        {
                            displayUpdate("Idle too long.", "Take Cup.");
                            brewerState = TAKE_CUP;
                        }

            }
            brewerState = TAKE_CUP; 
            
            break;
        case TAKE_CUP:
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
                    formatter_format_card_number(cardNumber, transaction.payment, sizeof(transaction.payment));
                }

                xQueueOverwrite(transaction_queue, &transaction);
                transaction_logged = 1;
            }
            
            //wait for signal from "cup sensor" (aka button input) that cup has been taken
            if(xQueueReceive(button_queue1, &key_buffer, portMAX_DELAY) == pdTRUE){ //just check if its been clicked
                brewerState = PRODUCT_SELECT; //back to start for next customer
                transaction_logged = 0;
            }

            break;

        default:
            //error code, should never happen
            break;
        }
    }
}
