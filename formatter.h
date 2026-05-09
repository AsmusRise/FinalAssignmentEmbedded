/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: formatter.h
*
* PROJECT....: Final Assignment
*
* DESCRIPTION: Formatting service for LCD and other user-facing text.
*****************************************************************************/

#ifndef FORMATTER_H_
#define FORMATTER_H_

/***************************** Include files *******************************/
#include "emp_type.h"

/*****************************    Defines    *******************************/
#define FORMATTER_TEXT_LENGTH 17

typedef enum
{
	FORMATTER_REQ_STARTUP = 0,
	FORMATTER_REQ_CHANGE,
	FORMATTER_REQ_CASH_STATUS,
	FORMATTER_REQ_PROGRESS,
	FORMATTER_REQ_CARD_NUMBER
} formatter_request_type_t;

typedef struct
{
	formatter_request_type_t type;
	INT16U espresso_price_dkk;
	INT16U latte_price_dkk;
	INT16U filter_price_per_cl_dkk;
	INT32U time_of_day_seconds;
	INT16U cash_dkk;
	INT16U amount_cl;
	INT32U total_price_tenths_dkk;
	INT64U card_number;
} formatter_request_t;

typedef struct
{
	char line1[FORMATTER_TEXT_LENGTH];
	char line2[FORMATTER_TEXT_LENGTH];
} formatter_result_t;

BOOLEAN formatter_format_startup(INT16U espresso_price_dkk,
								 INT16U latte_price_dkk,
								 INT16U filter_price_per_cl_dkk,
								 INT32U time_of_day_seconds,
								 char *line1,
								 char *line2);
BOOLEAN formatter_format_change(INT16U cash_dkk, char *line1, char *line2);
BOOLEAN formatter_format_cash_status(INT16U cash_dkk, char *line1, char *line2);
BOOLEAN formatter_format_progress(INT16U amount_cl,
						  INT16U unit_price_dkk,
						  INT32U total_price_tenths_dkk,
								  char *line1,
								  char *line2);
BOOLEAN formatter_format_card_number(INT64U card_number, char *dest, INT16U dest_len);

/****************************** End Of Module *******************************/
#endif /* FORMATTER_H_ */