#include "coffebrewer.h"
#include "queue.h"
#include "emp_type.h"
#include "status_led.h"
#include "color_led.h"
#include "adc.h"
#include "key.h"
#include "lcd.h"
#include "uart0.h"

extern QueueHandle_t button_queue;
extern QueueHandle_t greenQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t redQueue;
extern QueueHandle_t key_queue;
extern QueueHandle_t lcd_queue;
extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern QueueHandle_t adc_queue;
extern QueueHandle_t adc_to_uart_queue;

INT16U brewerState = PRODUCT_SELECT;
INT16U selectedProduct = 0;
INT16U timer1 = 0;
INT16U timer2 = 0;
INT16U timer3 = 0;
INT8U key_buffer = 0;
INT8U keyCounter = 0;
INT8U keylist[20]; 
INT64U cardNumber = 0;
INT16U sum = 0;
INT16U pincode = 0;


void timer_task(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1)
    {
        if(timer1 > 0)
        {
            timer1--;
        }
        if(timer2 > 0)
        {
            timer2--; 
        }
        if(timer3 > 0)
        {
            timer3--; 
        }
        vTaskDelayUntil(&xLastWakeTime, 10 / portTICK_RATE_MS);
    }
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
}

void coffebrewer_task(void *pvParameters)
{
    (void)pvParameters;
    while(1)
    {
        switch (brewerState)
        {
        case PRODUCT_SELECT:
            wr_str_LCD("Select Product:");
            move_LCD(0,1);
            wr_str_LCD("1:Espresso 2:Latte 3:Filter Coffee");

            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
            
                //button press received, state is now set to the button that was pressed
                switch (key_buffer)
                {
                case '1':
                    clr_LCD();
                    move_LCD(0,0);
                    wr_str_LCD("You selected: Espresso");
                    move_LCD(0,1);
                    wr_str_LCD("Confirm? #: Yes, *: No");

                    if(selectConfirm())
                    {
                        selectedProduct = ESPRESSO;
                        brewerState = PAYMENT_SELECT;
                    }
                    break;
                case '2':
                    //update display
                    if(selectConfirm())
                    {
                        selectedProduct = LATTE;
                        brewerState = PAYMENT_SELECT;
                    }
                    break;
                case '3':
                    //update display
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
             if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
            
                //button press received, state is now set to the button that was pressed
                switch (key_buffer)
                {
                case '1':
                   //update display
                    if(selectConfirm())
                    {
                        brewerState = CARD;
                    }
                    break;
                case '2':
                    //update display
                    if(selectConfirm())
                    {
                        brewerState = CASH;
                    }
                    break;
                default:
                    // not valid product, ignore
                    break;
                }
            }
            break;
        case CARD:
            //update display
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
                        if(selectConfirm())
                        {
                        
                            //convert to number and save for pin confirmation
                            for(int i = 0; i < keyCounter; i++)
                            {
                                cardNumber = cardNumber * 10 + (keylist[i] - '0');
                            }
                            //clear keylist and reset counter
                            for(int i = 0; i < keyCounter; i++)
                            {
                                keylist[i] = 0;
                            }
                            keyCounter = 0;
                            brewerState = PINCODE;
                        } else {
                            //clear keylist and reset counter
                            for(int i = 0; i < keyCounter; i++)
                            {
                                keylist[i] = 0;
                            }
                            keyCounter = 0;
                            //back to card number input, update display
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
                    for(int i = 0; i < keyCounter; i++)
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
                            brewerState = selectedProduct;//set to brewing state based on selected product
                        } else {
                            //update display with failure and return to product selection
                            brewerState = PRODUCT_SELECT;
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
                    for(int i = 0; i < keyCounter; i++)
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
        case CASH:
            //update display
            if(xQueueReceive(key_queue,  &key_buffer, portMAX_DELAY) == pdTRUE)
            {
                //cash inserted, update display
                if(selectConfirm())
                {
                    brewerState = CUP_PRESENCE;
                }
            }
            break;
        case CUP_PRESENCE:
            /* code */
            break;
        case ESPRESSO_BREWING:
            /* code */
            break;
        case LATTE_BREWING:
            /* code */
            break;
        case FILTER_COFFEE:
            /* code */
            break;
        case TAKE_CUP:
            /* code */
            break;
        
        default:
            //error code, should never happen
            break;
        }
    }
}