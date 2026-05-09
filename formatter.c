/*****************************************************************************
* University of Southern Denmark
* Embedded C Programming (ECP)
*
* MODULENAME.: formatter.c
*
* PROJECT....: Final Assignment
*
* DESCRIPTION: Formatting service task. Keeps printf/snprintf off the
*              coffebrewer task stack by handling all formatted text in a
*              dedicated task.
*****************************************************************************/

/***************************** Include files *******************************/
#include <string.h>
#include "formatter.h"

/*****************************   Functions   *******************************/
static void formatter_build_result(const formatter_request_t *request,
								   formatter_result_t *result);

static void formatter_exchange(const formatter_request_t *request,
								 formatter_result_t *result)
{
	formatter_build_result(request, result);
}

static char *formatter_append_char(char *dest, char *end, char ch)
{
	if(dest < end)
	{
		*dest = ch;
		dest++;
	}

	return dest;
}

static char *formatter_append_str(char *dest, char *end, const char *src)
{
	while((*src != '\0') && (dest < end))
	{
		*dest = *src;
		dest++;
		src++;
	}

	return dest;
}

static char *formatter_append_uint(char *dest, char *end, unsigned long value, INT8U min_width)
{
	char digits[10];
	INT8U digit_count;
	INT8U i;

	digit_count = 0;
	do
	{
		digits[digit_count] = (char)('0' + (value % 10UL));
		digit_count++;
		value /= 10UL;
	} while((value > 0UL) && (digit_count < (INT8U)sizeof(digits)));

	while(digit_count < min_width)
	{
		dest = formatter_append_char(dest, end, '0');
		min_width--;
	}

	for(i = digit_count; i > 0U; i--)
	{
		dest = formatter_append_char(dest, end, digits[i - 1U]);
	}

	return dest;
}

static char *formatter_append_u64(char *dest, char *end, unsigned long long value, INT8U min_width)
{
	char digits[20];
	INT8U digit_count;
	INT8U i;

	digit_count = 0;
	do
	{
		digits[digit_count] = (char)('0' + (value % 10ULL));
		digit_count++;
		value /= 10ULL;
	} while((value > 0ULL) && (digit_count < (INT8U)sizeof(digits)));

	while(digit_count < min_width)
	{
		dest = formatter_append_char(dest, end, '0');
		min_width--;
	}

	for(i = digit_count; i > 0U; i--)
	{
		dest = formatter_append_char(dest, end, digits[i - 1U]);
	}

	return dest;
}

static void formatter_finalize(char *cursor, char *end)
{
	if(cursor <= end)
	{
		*cursor = '\0';
	}
	else
	{
		end[0] = '\0';
	}
}

static void formatter_write_startup(const formatter_request_t *request,
									formatter_result_t *result)
{
	char *cursor;
	char *end;
	INT32U hours;
	INT32U minutes;
	INT32U seconds;

	cursor = result->line1;
	end = result->line1 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "E:");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->espresso_price_dkk, 1U);
	cursor = formatter_append_str(cursor, end, " L:");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->latte_price_dkk, 1U);
	cursor = formatter_append_str(cursor, end, " F:");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->filter_price_per_cl_dkk, 1U);
	formatter_finalize(cursor, end);

	hours = request->time_of_day_seconds / 3600UL;
	minutes = (request->time_of_day_seconds % 3600UL) / 60UL;
	seconds = request->time_of_day_seconds % 60UL;
	cursor = result->line2;
	end = result->line2 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "T:");
	cursor = formatter_append_uint(cursor, end, (unsigned long)hours, 2U);
	cursor = formatter_append_char(cursor, end, ':');
	cursor = formatter_append_uint(cursor, end, (unsigned long)minutes, 2U);
	cursor = formatter_append_char(cursor, end, ':');
	cursor = formatter_append_uint(cursor, end, (unsigned long)seconds, 2U);
	formatter_finalize(cursor, end);
}

static void formatter_write_change(const formatter_request_t *request,
								 formatter_result_t *result)
{
	char *cursor;
	char *end;

	cursor = result->line1;
	end = result->line1 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "Change: ");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->cash_dkk, 1U);
	cursor = formatter_append_str(cursor, end, " DKK");
	formatter_finalize(cursor, end);
	result->line2[0] = '\0';
}

static void formatter_write_cash_status(const formatter_request_t *request,
									formatter_result_t *result)
{
	char *cursor;
	char *end;

	cursor = result->line1;
	end = result->line1 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "Cash: ");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->cash_dkk, 1U);
	cursor = formatter_append_str(cursor, end, " DKK");
	formatter_finalize(cursor, end);
	result->line2[0] = '\0';
}

static void formatter_write_progress(const formatter_request_t *request,
								 formatter_result_t *result)
{
	char *cursor;
	char *end;
	INT32U total_whole;

	/* total_price_tenths_dkk stores tenths of DKK; round to nearest DKK */
	total_whole = (request->total_price_tenths_dkk + 5U) / 10U;
	cursor = result->line1;
	end = result->line1 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "Amt: ");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->amount_cl, 1U);
	cursor = formatter_append_str(cursor, end, " cl U:");
	cursor = formatter_append_uint(cursor, end, (unsigned long)request->filter_price_per_cl_dkk, 1U);
	formatter_finalize(cursor, end);

	cursor = result->line2;
	end = result->line2 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_str(cursor, end, "Total: ");
	cursor = formatter_append_uint(cursor, end, (unsigned long)total_whole, 1U);
	cursor = formatter_append_str(cursor, end, " DKK");
	formatter_finalize(cursor, end);
}

static void formatter_write_card_number(const formatter_request_t *request,
								   formatter_result_t *result)
{
	char *cursor;
	char *end;

	cursor = result->line1;
	end = result->line1 + FORMATTER_TEXT_LENGTH - 1U;
	cursor = formatter_append_u64(cursor, end, (unsigned long long)request->card_number, 16U);
	formatter_finalize(cursor, end);
	result->line2[0] = '\0';
}

BOOLEAN formatter_format_startup(INT16U espresso_price_dkk,
								 INT16U latte_price_dkk,
								 INT16U filter_price_per_cl_dkk,
								 INT32U time_of_day_seconds,
								 char *line1,
								 char *line2)
{
	formatter_request_t request;
	formatter_result_t result;

	line1[0] = '\0';
	line2[0] = '\0';

	request.type = FORMATTER_REQ_STARTUP;
	request.espresso_price_dkk = espresso_price_dkk;
	request.latte_price_dkk = latte_price_dkk;
	request.filter_price_per_cl_dkk = filter_price_per_cl_dkk;
	request.time_of_day_seconds = time_of_day_seconds;
	request.cash_dkk = 0;
	request.amount_cl = 0;
	request.total_price_tenths_dkk = 0;
	request.card_number = 0;

	formatter_exchange(&request, &result);
	strncpy(line1, result.line1, FORMATTER_TEXT_LENGTH);
	line1[FORMATTER_TEXT_LENGTH - 1] = '\0';
	strncpy(line2, result.line2, FORMATTER_TEXT_LENGTH);
	line2[FORMATTER_TEXT_LENGTH - 1] = '\0';

	return 1;
}

BOOLEAN formatter_format_change(INT16U cash_dkk, char *line1, char *line2)
{
	formatter_request_t request;
	formatter_result_t result;

	line1[0] = '\0';
	line2[0] = '\0';

	request.type = FORMATTER_REQ_CHANGE;
	request.espresso_price_dkk = 0;
	request.latte_price_dkk = 0;
	request.filter_price_per_cl_dkk = 0;
	request.time_of_day_seconds = 0;
	request.cash_dkk = cash_dkk;
	request.amount_cl = 0;
	request.total_price_tenths_dkk = 0;
	request.card_number = 0;

	formatter_exchange(&request, &result);
	strncpy(line1, result.line1, FORMATTER_TEXT_LENGTH);
	line1[FORMATTER_TEXT_LENGTH - 1] = '\0';
	strncpy(line2, result.line2, FORMATTER_TEXT_LENGTH);
	line2[FORMATTER_TEXT_LENGTH - 1] = '\0';

	return 1;
}

BOOLEAN formatter_format_cash_status(INT16U cash_dkk, char *line1, char *line2)
{
	formatter_request_t request;
	formatter_result_t result;

	line1[0] = '\0';
	line2[0] = '\0';

	request.type = FORMATTER_REQ_CASH_STATUS;
	request.espresso_price_dkk = 0;
	request.latte_price_dkk = 0;
	request.filter_price_per_cl_dkk = 0;
	request.time_of_day_seconds = 0;
	request.cash_dkk = cash_dkk;
	request.amount_cl = 0;
	request.total_price_tenths_dkk = 0;
	request.card_number = 0;

	formatter_exchange(&request, &result);
	strncpy(line1, result.line1, FORMATTER_TEXT_LENGTH);
	line1[FORMATTER_TEXT_LENGTH - 1] = '\0';
	strncpy(line2, result.line2, FORMATTER_TEXT_LENGTH);
	line2[FORMATTER_TEXT_LENGTH - 1] = '\0';

	return 1;
}

BOOLEAN formatter_format_progress(INT16U amount_cl,
								  INT16U unit_price_dkk,
					  INT32U total_price_tenths_dkk,
					  char *line1,
					  char *line2)
{
	formatter_request_t request;
	formatter_result_t result;

	line1[0] = '\0';
	line2[0] = '\0';

	request.type = FORMATTER_REQ_PROGRESS;
	request.espresso_price_dkk = 0;
	request.latte_price_dkk = 0;
	request.filter_price_per_cl_dkk = unit_price_dkk;
	request.time_of_day_seconds = 0;
	request.cash_dkk = 0;
	request.amount_cl = amount_cl;
	request.total_price_tenths_dkk = total_price_tenths_dkk;
	request.card_number = 0;

	formatter_exchange(&request, &result);
	strncpy(line1, result.line1, FORMATTER_TEXT_LENGTH);
	line1[FORMATTER_TEXT_LENGTH - 1] = '\0';
	strncpy(line2, result.line2, FORMATTER_TEXT_LENGTH);
	line2[FORMATTER_TEXT_LENGTH - 1] = '\0';

	return 1;
}

BOOLEAN formatter_format_card_number(INT64U card_number, char *dest, INT16U dest_len)
{
	formatter_request_t request;
	formatter_result_t result;
	INT16U copy_len;

	if(dest_len > 0)
	{
		dest[0] = '\0';
	}

	request.type = FORMATTER_REQ_CARD_NUMBER;
	request.espresso_price_dkk = 0;
	request.latte_price_dkk = 0;
	request.filter_price_per_cl_dkk = 0;
	request.time_of_day_seconds = 0;
	request.cash_dkk = 0;
	request.amount_cl = 0;
	request.total_price_tenths_dkk = 0;
	request.card_number = card_number;

	formatter_exchange(&request, &result);
	if(dest_len > 0)
	{
		copy_len = (dest_len - 1 < FORMATTER_TEXT_LENGTH - 1) ? (dest_len - 1) : (FORMATTER_TEXT_LENGTH - 1);
		strncpy(dest, result.line1, copy_len);
		dest[copy_len] = '\0';
	}

	return 1;
}

static void formatter_build_result(const formatter_request_t *request,
								   formatter_result_t *result)
{
	result->line1[0] = '\0';
	result->line2[0] = '\0';

	switch(request->type)
	{
		case FORMATTER_REQ_STARTUP:
			formatter_write_startup(request, result);
			break;

		case FORMATTER_REQ_CHANGE:
			formatter_write_change(request, result);
			break;

		case FORMATTER_REQ_CASH_STATUS:
			formatter_write_cash_status(request, result);
			break;

		case FORMATTER_REQ_PROGRESS:
			formatter_write_progress(request, result);
			break;

		case FORMATTER_REQ_CARD_NUMBER:
			formatter_write_card_number(request, result);
			break;

		default:
			result->line1[0] = '\0';
			result->line2[0] = '\0';
			break;
	}
}

/****************************** End Of Module *******************************/