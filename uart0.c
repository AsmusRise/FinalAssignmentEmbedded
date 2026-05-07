/*****************************************************************************
* University of Southern Denmark
* Embedded Programming (EMP)
*
* MODULENAME.: emp.c
*
* PROJECT....: EMP
*
* DESCRIPTION: See module specification file (.h-file).
*
* Change Log:
*****************************************************************************
* Date    Id    Change
* YYMMDD
* --------------------
* 150228  MoH   Module created.
*
*****************************************************************************/

/***************************** Include files *******************************/
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "emp_type.h"
#include "string.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include "task.h"
#include <stdio.h>


extern QueueHandle_t uart_tx_queue;
extern QueueHandle_t uart_rx_queue;
extern SemaphoreHandle_t xSemaphore;
/*****************************    Defines    *******************************/

/*****************************   Constants   *******************************/

/*****************************   Variables   *******************************/


/*****************************   Functions   *******************************/

static BOOLEAN uart_read_line(char *dst, INT16U dst_len)
{
  INT16U idx = 0;
  INT8U ch = 0;

  if(dst_len == 0)
  {
    return 0;
  }

  while(1)
  {
    if(xQueueReceive(uart_rx_queue, &ch, 1000 / portTICK_RATE_MS) != pdTRUE)
    {
      return 0;
    }

    if(ch == '\r')
    {
      dst[idx] = '\0';
      return 1;
    }

    if(ch == '\n')
    {
      continue;
    }

    if(idx < (dst_len - 1))
    {
      dst[idx] = (char)ch;
      idx++;
    }
  }
}

static BOOLEAN parse_uint16_from_line(const char *line, INT16U *value, BOOLEAN *has_value)
{
  INT32U parsed = 0;
  INT16U i = 0;

  while(line[i] == ' ' || line[i] == '\t')
  {
    i++;
  }

  if(line[i] == '\0')
  {
    *has_value = 0;
    return 1;
  }

  *has_value = 1;
  while(line[i] >= '0' && line[i] <= '9')
  {
    parsed = (parsed * 10U) + (INT32U)(line[i] - '0');
    if(parsed > 65535U)
    {
      return 0;
    }
    i++;
  }

  while(line[i] == ' ' || line[i] == '\t')
  {
    i++;
  }

  if(line[i] != '\0')
  {
    return 0;
  }

  *value = (INT16U)parsed;
  return 1;
}

static BOOLEAN parse_hms_from_line(const char *line, INT32U *seconds_of_day, BOOLEAN *has_value)
{
  INT16U hh = 0;
  INT16U mm = 0;
  INT16U ss = 0;
  INT16U i = 0;
  char trailing = 0;

  while(line[i] == ' ' || line[i] == '\t')
  {
    i++;
  }

  if(line[i] == '\0')
  {
    *has_value = 0;
    return 1;
  }

  *has_value = 1;

  if(sscanf(&line[i], "%hu:%hu:%hu %c", &hh, &mm, &ss, &trailing) != 3)
  {
    return 0;
  }

  if(hh > 23U || mm > 59U || ss > 59U)
  {
    return 0;
  }

  *seconds_of_day = (INT32U)hh * 3600U + (INT32U)mm * 60U + (INT32U)ss;
  return 1;
}

void uart0_read_startup_config(INT16U *espresso_price,
                               INT16U *latte_price,
                               INT16U *filter_price,
                               INT32U *time_of_day_seconds)
{
  char rx_line[32];
  INT16U parsed_price = 0;
  INT32U parsed_time_seconds = 0;
  BOOLEAN has_value = 0;

  xQueueReset(uart_rx_queue);

  uart0_puts_selfmade((INT8U*)"Startup setup over UART");
  uart0_puts_selfmade((INT8U*)"Send 4 lines, each ended by CR:");
  uart0_puts_selfmade((INT8U*)"1) Espresso price DKK (empty=15)");
  uart0_puts_selfmade((INT8U*)"2) Latte price DKK (empty=27)");
  uart0_puts_selfmade((INT8U*)"3) Filter price DKK/cl (empty=3)");
  uart0_puts_selfmade((INT8U*)"4) Time HH:MM:SS (empty=00:00:00)");

  if(uart_read_line(rx_line, sizeof(rx_line)) == 1)
  {
    if(parse_uint16_from_line(rx_line, &parsed_price, &has_value) == 1 && has_value == 1 && espresso_price != NULL)
    {
      *espresso_price = parsed_price;
    }
  }

  if(uart_read_line(rx_line, sizeof(rx_line)) == 1)
  {
    if(parse_uint16_from_line(rx_line, &parsed_price, &has_value) == 1 && has_value == 1 && latte_price != NULL)
    {
      *latte_price = parsed_price;
    }
  }

  if(uart_read_line(rx_line, sizeof(rx_line)) == 1)
  {
    if(parse_uint16_from_line(rx_line, &parsed_price, &has_value) == 1 && has_value == 1 && filter_price != NULL)
    {
      *filter_price = parsed_price;
    }
  }

  if(uart_read_line(rx_line, sizeof(rx_line)) == 1)
  {
    if(parse_hms_from_line(rx_line, &parsed_time_seconds, &has_value) == 1 && has_value == 1)
    {
      if(time_of_day_seconds != NULL)
      {
        *time_of_day_seconds = parsed_time_seconds;
      }
    }
    else if(time_of_day_seconds != NULL)
    {
      *time_of_day_seconds = 0;
    }
  }

  uart0_puts_selfmade((INT8U*)"UART setup complete.");
}


INT32U lcrh_databits( INT8U antal_databits )
/*****************************************************************************
*   Input    :
*   Output   :
*   Function : sets bit 5 and 6 according to the wanted number of data bits.
*   		    5: bit5 = 0, bit6 = 0.
*   		    6: bit5 = 1, bit6 = 0.
*   		    7: bit5 = 0, bit6 = 1.
*   		    8: bit5 = 1, bit6 = 1  (default).
*   		   all other bits are returned = 0
******************************************************************************/
{
  if(( antal_databits < 5 ) || ( antal_databits > 8 ))
	antal_databits = 8;
  return(( (INT32U)antal_databits - 5 ) << 5 );  // Control bit 5-6, WLEN
}

INT32U lcrh_stopbits( INT8U antal_stopbits )
/*****************************************************************************
*   Input    :
*   Output   :
*   Function : sets bit 3 according to the wanted number of stop bits.
*   		    1 stpobit:  bit3 = 0 (default).
*   		    2 stopbits: bit3 = 1.
*   		   all other bits are returned = 0
******************************************************************************/
{
  if( antal_stopbits == 2 )
    return( 0x00000008 );  		// return bit 3 = 1
  else
	return( 0x00000000 );		// return all zeros
}

INT32U lcrh_parity( INT8U parity )
/*****************************************************************************
*   Input    :
*   Output   :
*   Function : sets bit 1, 2 and 7 to the wanted parity.
*   		    'e':  00000110b.
*   		    'o':  00000010b.
*   		    '0':  10000110b.
*   		    '1':  10000010b.
*   		    'n':  00000000b.
*   		   all other bits are returned = 0
******************************************************************************/
{
  INT32U result;

  switch( parity )
  {
    case 'e':
      result = 0x00000006;
      break;
    case 'o':
      result = 0x00000002;
      break;
    case '0':
      result = 0x00000086;
      break;
    case '1':
      result = 0x00000082;
      break;
    case 'n':
    default:
      result = 0x00000000;
  }
  return( result );
}

void uart0_fifos_enable()
/*****************************************************************************
*   Input    :
*   Output   :
*   Function : Enable the tx and rx fifos
******************************************************************************/
{
  UART0_LCRH_R  |= 0x00000010;
}

void uart0_fifos_disable()
/*****************************************************************************
*   Input    :
*   Output   :
*   Function : Enable the tx and rx fifos
******************************************************************************/
{
  UART0_LCRH_R  &= 0xFFFFFFEF;
}


extern BOOLEAN uart0_rx_rdy()
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  return( UART0_FR_R & UART_FR_RXFF );
}

extern INT8U uart0_getc()
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  return ( UART0_DR_R );
}

extern BOOLEAN uart0_tx_rdy()
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  return( UART0_FR_R & UART_FR_TXFE );
}

extern void uart0_putc( INT8U ch )
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  UART0_DR_R = ch;
}

/*********************  Queue-based TX/RX helpers  *************************/

extern void uart0_puts( INT8U *str )
/*****************************************************************************
*   Function : Send string + CR+LF via TX queue (non-blocking for caller).
*****************************************************************************/
{
  while( *str )
  {
    xQueueSend( uart_tx_queue, str, portMAX_DELAY );
    str++;
  }
  INT8U cr = '\r';
  INT8U lf = '\n';
  xQueueSend( uart_tx_queue, &cr, portMAX_DELAY );
  xQueueSend( uart_tx_queue, &lf, portMAX_DELAY );
}

extern void uart0_puts_selfmade( INT8U *str )
/*****************************************************************************
*   Function : Send string + CR+LF via TX queue (non-blocking for caller).
*****************************************************************************/
{
  int length = strlen((const char*)str);
  int i;
  for(i = 0; i < length; i++)
  {
    xQueueSend( uart_tx_queue, &str[i], portMAX_DELAY );
  }
  INT8U cr = '\r';
  INT8U lf = '\n';
  xQueueSend( uart_tx_queue, &cr, portMAX_DELAY );
  xQueueSend( uart_tx_queue, &lf, portMAX_DELAY );
}


extern void uart0_init( INT32U baud_rate, INT8U databits, INT8U stopbits, INT8U parity )
/*****************************************************************************
*   Function : See module specification (.h-file).
*****************************************************************************/
{
  INT32U BRD;

  #ifndef E_PORTA
  #define E_PORTA
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA;					// Enable clock for Port A
  #endif

  #ifndef E_UART0
  #define E_UART0
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_UART0;					// Enable clock for UART 0
  #endif

  GPIO_PORTA_AFSEL_R |= 0x00000003;		// set PA0 og PA1 to alternativ function (uart0)
  GPIO_PORTA_DIR_R   |= 0x00000002;     // set PA1 (uart0 tx) to output
  GPIO_PORTA_DIR_R   &= 0xFFFFFFFE;     // set PA0 (uart0 rx) to input
  GPIO_PORTA_DEN_R   |= 0x00000003;		// enable digital operation of PA0 and PA1
  //GPIO_PORTA_PUR_R   |= 0x00000002;

  BRD = 64000000 / baud_rate;   	// X-sys*64/(16*baudrate) = 16M*4/baudrate
  UART0_IBRD_R = BRD / 64;
  UART0_FBRD_R = BRD & 0x0000003F;

  UART0_LCRH_R  = lcrh_databits( databits );
  UART0_LCRH_R += lcrh_stopbits( stopbits );
  UART0_LCRH_R += lcrh_parity( parity );

  uart0_fifos_disable();

  UART0_CTL_R  |= (UART_CTL_UARTEN | UART_CTL_TXE | UART_CTL_RXE);  // Enable UART
}

/****************************** End Of Module *******************************/


/****************************  FreeRTOS Tasks  *****************************/

void uart_tx_task( void *pvParameters )
/*****************************************************************************
*   Input    : pvParameters (unused)
*   Output   : -
*   Function : FreeRTOS task - blocks on uart_tx_queue, sends each byte
*              to hardware when TX is ready.
*****************************************************************************/
{
  INT8U ch;

  while(1)
  {
    if( xQueueReceive( uart_tx_queue, &ch, portMAX_DELAY ) == pdTRUE )
    {
      while( !uart0_tx_rdy() );  // brief busy-wait for hardware (microseconds)
      uart0_putc( ch );
    }
  }
}

void uart_rx_task( void *pvParameters )
/*****************************************************************************
*   Input    : pvParameters (unused)
*   Output   : -
*   Function : FreeRTOS task - polls UART RX at 10ms intervals, pushes
*              received bytes into uart_rx_queue for other tasks to consume.
*****************************************************************************/
{
  INT8U ch;

  while(1)
  {
    while( uart0_rx_rdy() )
    {
      ch = uart0_getc();
      xQueueSend( uart_rx_queue, &ch, 0 );  // drop if queue full
    }
    vTaskDelay( 10 / portTICK_RATE_MS );
  }
}

