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
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

extern QueueHandle_t uart_rx_queue;

/*****************************   Variables   *******************************/

// In-memory database: linear buffer of transaction records
static transaction_t db[LOGGER_DB_SIZE];
static INT8U db_count = 0;    /* current number of valid records */
static INT32U db_first_time = 0; /* uptime of first transaction (for operating time calc) */

// Transaction queue is created/defined by main.c
extern QueueHandle_t transaction_queue;

/*****************************   Functions   *******************************/

INT8U logger_db_count(void)
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
/* Lightweight helpers (no snprintf) */
static char *log_append_char(char *dest, char *end, char ch)
{
  if(dest < end) { *dest++ = ch; }
  return dest;
}

static char *log_append_str(char *dest, char *end, const char *src)
{
  while(*src && dest < end) { *dest++ = *src++; }
  return dest;
}

static char *log_append_uint(char *dest, char *end, unsigned long value, INT8U min_width)
{
  char digits[10];
  INT8U count = 0;
  INT8U i;

  do {
    digits[count++] = '0' + (char)(value % 10);
    value /= 10;
  } while(value > 0);

  while(count < min_width && count < sizeof(digits))
  {
    digits[count++] = '0';
  }

  for(i = count; i > 0; i--)
  {
    dest = log_append_char(dest, end, digits[i - 1]);
  }
  return dest;
}

BOOLEAN logger_format_transaction(transaction_t *rec, INT8U *buffer, INT8U buffer_len)
{
  const char *product_names[] = {"FILTER", "LATTE", "ESPRESSO"};
  char *cursor = (char *)buffer;
  char *end = (char *)buffer + buffer_len - 1;

  if(rec->product > 9 || rec->product < 7) return 0;

  INT8U product = 9 - rec->product;

  INT32U hours = rec->uptime_sec / 3600;
  INT32U mins = (rec->uptime_sec % 3600) / 60;
  INT32U secs = rec->uptime_sec % 60;

  /* HH:MM:SS */
  cursor = log_append_uint(cursor, end, hours, 2);
  cursor = log_append_char(cursor, end, ':');
  cursor = log_append_uint(cursor, end, mins, 2);
  cursor = log_append_char(cursor, end, ':');
  cursor = log_append_uint(cursor, end, secs, 2);

  /* ,PRODUCT */
  cursor = log_append_char(cursor, end, ',');
  cursor = log_append_str(cursor, end, product_names[product]);

  /* ,PRICE */
  cursor = log_append_char(cursor, end, ',');
  cursor = log_append_uint(cursor, end, (unsigned long)rec->price, 1);

  /* ,AMOUNT */
  cursor = log_append_char(cursor, end, ',');
  cursor = log_append_uint(cursor, end, (unsigned long)rec->amount, 1);

  /* ,PAYMENT */
  cursor = log_append_char(cursor, end, ',');
  cursor = log_append_str(cursor, end, rec->payment);

  *cursor = '\0';
  return (cursor > (char *)buffer) ? 1 : 0;
}

/* Check if a "log\r" command has been received on UART RX.
 * Non-blocking: returns 1 if full command matched, 0 otherwise.
 */
static BOOLEAN check_uart_log_command(void)
{
  static char cmd_buf[4];
  static INT8U cmd_idx = 0;
  INT8U ch;

  while(xQueueReceive(uart_rx_queue, &ch, 0) == pdTRUE)
  {
    if(ch == '\r' || ch == '\n')
    {
      if(cmd_idx == 3 &&
         cmd_buf[0] == 'l' && cmd_buf[1] == 'o' && cmd_buf[2] == 'g')
      {
        cmd_idx = 0;
        return 1;
      }
      cmd_idx = 0;
    }
    else
    {
      if(cmd_idx < 3)
      {
        cmd_buf[cmd_idx] = (char)ch;
      }
      cmd_idx++;
    }
  }
  return 0;
}

static void logger_send_transaction_uart(transaction_t *rec)
{
  INT8U buf[80];
  if(logger_format_transaction(rec, buf, sizeof(buf)))
  {
    uart0_puts_selfmade(buf);
  }
}

static void logger_dump_all_uart(void)
{
  INT8U i;
  transaction_t rec;

  uart0_puts_selfmade((INT8U*)"--- Transaction Log ---");
  uart0_puts_selfmade((INT8U*)"TIME,PRODUCT,PRICE,AMOUNT,PAYMENT");

  for(i = 0; i < db_count; i++)
  {
    if(logger_db_get(i, &rec))
    {
      logger_send_transaction_uart(&rec);
    }
  }

  uart0_puts_selfmade((INT8U*)"--- End of Log ---");
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
    /* Wait for a transaction, but wake periodically to check UART commands */
    if(xQueueReceive(transaction_queue, &rec, 200 / portTICK_RATE_MS) == pdTRUE)
    {
      /* Only store if database is not full */
      if(db_count < LOGGER_DB_SIZE)
      {
        if(db_count == 0)
        {
          db_first_time = rec.uptime_sec;
        }

        db[db_count] = rec;
        db_count++;
      }

      /* Send this transaction to UART */
      logger_send_transaction_uart(&rec);
    }

    /* Check for "log" command on UART */
    if(check_uart_log_command())
    {
      logger_dump_all_uart();
    }
  }
}

/****************************** End Of Module *******************************/
