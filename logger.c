/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: logger.c
*
* PROJECT....: EMP Coffee Brewer
*
* DESCRIPTION: Transaction logging module using FreeRTOS queue and RAM DB.
*
*****************************************************************************/

/***************************** Include files *******************************/
#include "logger.h"
#include "tm4c123gh6pm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

/*****************************   Variables   *******************************/

// In-memory database: linear buffer of transaction records
static transaction_t db[LOGGER_DB_SIZE];
static INT8U db_count = 0;    /* current number of valid records */
static INT32U db_first_time = 0; /* uptime of first transaction (for operating time calc) */

// Transaction queue is created/defined by main.c
extern QueueHandle_t transaction_queue;

/*****************************   Functions   *******************************/

INT16U logger_db_count(void)
{
  return db_count;
}

BOOLEAN logger_db_get(INT8U index, transaction_t *out_record)
{
  if(index >= db_count) return 0;
  *out_record = db[index];
  return 1;
}

BOOLEAN logger_db_is_full(void)
{
  return (db_count >= LOGGER_DB_SIZE);
}

INT64U logger_query_total_sales_by_product(INT8U product)
{
  INT64U total_price = 0;
  INT8U i;

  for(i = 0; i < db_count; i++)
  {
    if(db[i].product == product)
    {
      total_price += (INT64U)db[i].price * (INT64U)db[i].amount;
    }
  }

  return total_price;
}

INT64U logger_query_total_cash(void)
{
  INT64U total_price = 0;
  INT8U i;

  for(i = 0; i < db_count; i++)
  {
    if(strcmp(db[i].payment, "CASH") == 0)
    {
      total_price += (INT64U)db[i].price * (INT64U)db[i].amount;
    }
  }

  return total_price;
}

INT64U logger_query_total_card(void)
{
  INT64U total_price = 0;
  INT8U i;

  for(i = 0; i < db_count; i++)
  {
    if(strcmp(db[i].payment, "CASH") != 0)
    {
      total_price += (INT64U)db[i].price * (INT64U)db[i].amount;
    }
  }

  return total_price;
}

INT32U logger_query_operating_time_sec(void)
{
  if(db_count == 0) return 0;
  return (db[db_count - 1].uptime_sec - db_first_time);
}

/* Format transaction as string for UART output.
 * Writes formatted string to buffer in format: HH:MM:SS,PRODUCT,PRICE,AMOUNT,PAYMENT
 * Returns 1 if successful, 0 if buffer too small.
 * NOTE: Does NOT write to UART; caller is responsible for sending the formatted string.
 */
BOOLEAN logger_format_transaction(transaction_t *rec, INT8U *buffer, INT16U buffer_len)
{
  const char *product_names[] = {"FILTER", "LATTE", "ESPRESSO"};
  INT16U printed;

  if(rec->product > 9 || rec->product < 7) return 0;  /* invalid product */

  INT8U product = 9 - rec->product;

  /* Format: HH:MM:SS,PRODUCT,PRICE,AMOUNT,PAYMENT */
  INT32U hours = rec->uptime_sec / 3600;
  INT32U mins = (rec->uptime_sec % 3600) / 60;
  INT32U secs = rec->uptime_sec % 60;

  printed = snprintf((char *)buffer, buffer_len, "%02ld:%02ld:%02ld,%s,%u,%u,%s",
                     hours, mins, secs,
                     product_names[product],
                     rec->price,
                     rec->amount,
                     rec->payment);

  return (printed > 0 && printed < buffer_len) ? 1 : 0;
}

void log_task(void *pvParameters)
{
  transaction_t rec;

  (void)pvParameters;

  /* Initialize database */
  db_count = 0;
  db_first_time = 0;
  memset(db, 0, sizeof(db));

  while(1)
  {
    /* Wait for a transaction on the queue (blocks indefinitely until one arrives) */
    if(xQueueReceive(transaction_queue, &rec, portMAX_DELAY) == pdTRUE)
    {
      /* Only store if database is not full */
      if(db_count < LOGGER_DB_SIZE)
      {
        /* Record first transaction time for operating time calculation */
        if(db_count == 0)
        {
          db_first_time = rec.uptime_sec;
        }

        db[db_count] = rec;
        db_count++;
      }
      /* If database is full, transaction is silently dropped (fail case per requirements) */
    }
  }
}

/****************************** End Of Module *******************************/
