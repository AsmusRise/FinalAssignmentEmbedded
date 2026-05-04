/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: encoder.h
*
* PROJECT....: EMP FreeRTOS Pot Scaler
*
* DESCRIPTION: Rotary encoder driver for the EMP board.
*              Encoder channels:  PA5 = DIGI A,  PA6 = DIGI B
*****************************************************************************/
#ifndef ENCODER_H_
#define ENCODER_H_

#include "emp_type.h"

void   init_encoder(void);
INT32S encoder_read(void);   /* returns accumulated position (ticks) */
BOOLEAN get_encoder(INT32S *position);
void encoder_task(void *pvParameters);

#endif /* ENCODER_H_ */