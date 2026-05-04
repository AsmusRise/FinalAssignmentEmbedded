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
INT8U encoder_read(void);   /* returns step delta: 0=no change, 1=right, 2=left */
BOOLEAN get_encoder(INT8U *position);
void encoder_task(void *pvParameters);

#endif /* ENCODER_H_ */