/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: logger.h
*
* PROJECT....: EMP Coffee Brewer
*
* DESCRIPTION: Transaction logging module using FreeRTOS queue and RAM DB.
*
*****************************************************************************/

#ifndef _LOGGER_H
#define _LOGGER_H

#include "emp_type.h"
#include "FreeRTOS.h"
#include "queue.h"

/*****************************    Defines    *******************************/

// Product types (matches coffebrewer.h definitions)
typedef enum {
  PROD_ESPRESSO = 7,
  PROD_LATTE    = 8,
  PROD_FILTER   = 9
} product_t;

// Transaction record (raw data, stored in RAM DB)
typedef struct {
  INT32U uptime_sec;       /* seconds since system boot */
  INT8U  product;          /* product_t enum value */
  INT16U price;            /* price in DKK */
  INT8U  amount;           /* 1 for espresso/latte, centiliters for filter */
  char   payment[17];      /* "CASH" or 16-digit card number + null terminator */
} transaction_t;

#define LOGGER_DB_SIZE 255     /* maximum value of INT8U */

/*****************************   Functions   *******************************/

extern QueueHandle_t transaction_queue;

void log_task(void *pvParameters);

/* Database query functions */
INT8U logger_db_count(void);
BOOLEAN logger_db_get(INT8U index, transaction_t *out_record);
BOOLEAN logger_db_is_full(void);

/* Query functions for report generation */
INT64U logger_query_total_sales_by_product(INT8U product);
INT64U logger_query_total_cash(void);
INT64U logger_query_total_card(void);
INT32U logger_query_operating_time_sec(void);

/* Format transaction as string for UART output */
BOOLEAN logger_format_transaction(transaction_t *rec, INT8U *buffer, INT8U buffer_len);

#endif /* _LOGGER_H */
