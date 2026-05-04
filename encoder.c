/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: encoder.c
*
* PROJECT....: EMP FreeRTOS Pot Scaler
*
* DESCRIPTION: Quadrature rotary encoder driver for the EMP board.
*              FreeRTOS task polls the encoder and publishes step deltas.
*
*   From EMP pinout:
*     PA5 = DIGI A  (encoder channel A)
*     PA6 = DIGI B  (encoder channel B)
*****************************************************************************/

/***************************** Include files *******************************/
#include "encoder.h"
#include "tm4c123gh6pm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*****************************    Defines    *******************************/
#define ENC_A_PIN   (1U << 5)    /* PA5 */
#define ENC_B_PIN   (1U << 6)    /* PA6 */
#define ENC_PINS    (ENC_A_PIN | ENC_B_PIN)
#define ENC_COUNTS_PER_STEP  4

/*****************************   Variables   *******************************/
static INT8U  enc_prev_state = 0;
static INT8S  enc_step_accum = 0;

extern QueueHandle_t encoder_queue;

/*****************************   Constants   *******************************/
static const INT8S qei_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0,
};

/*****************************   Functions   *******************************/

void init_encoder(void)
{
    volatile INT32U dummy;

    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA;
    dummy = SYSCTL_RCGC2_R;
    (void)dummy;

    GPIO_PORTA_DIR_R  &= ~ENC_PINS;
    GPIO_PORTA_DEN_R  |=  ENC_PINS;
    GPIO_PORTA_AFSEL_R &= ~ENC_PINS;
    GPIO_PORTA_AMSEL_R &= ~ENC_PINS;
    GPIO_PORTA_PCTL_R  &=  0xF00FFFFF;
    GPIO_PORTA_PUR_R  |=  ENC_PINS;

    INT8U raw = GPIO_PORTA_DATA_R;
    enc_prev_state = 0;
    if (raw & ENC_A_PIN) enc_prev_state |= 0x02;
    if (raw & ENC_B_PIN) enc_prev_state |= 0x01;
    enc_step_accum = 0;
}

INT32S encoder_read(void)
{
    INT8U raw = GPIO_PORTA_DATA_R;
    INT8U new_state = 0;
    if (raw & ENC_A_PIN) new_state |= 0x02;
    if (raw & ENC_B_PIN) new_state |= 0x01;

    INT8U idx = (enc_prev_state << 2) | new_state;
    enc_step_accum += qei_table[idx];
    enc_prev_state = new_state;

    if(enc_step_accum >= ENC_COUNTS_PER_STEP)
    {
        enc_step_accum = 0;
        return 1;
    }

    if(enc_step_accum <= -ENC_COUNTS_PER_STEP)
    {
        enc_step_accum = 0;
        return -1;
    }

    return 0;
}

BOOLEAN get_encoder(INT32S *position)
{
    return (xQueueReceive(encoder_queue, position, portMAX_DELAY) == pdTRUE);
}

void encoder_task(void *pvParameters)
{
    INT32S position;

    (void)pvParameters;
    init_encoder();

    while(1)
    {
        position = encoder_read();
        if(position != 0)
        {
            xQueueOverwrite(encoder_queue, &position);
        }
        vTaskDelay(5 / portTICK_RATE_MS);
    }
}

/****************************** End Of Module *******************************/