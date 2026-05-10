/*****************************************************************************
* FreeRTOSConfig.h — Configuration for FreeRTOS on TM4C123GH6PM (16 MHz)
*
* NOTE: The default clock on the TM4C123 LaunchPad without PLL setup is
*       16 MHz (internal oscillator). If you enable PLL for 80 MHz,
*       change configCPU_CLOCK_HZ accordingly.
*****************************************************************************/
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ── Clock ─────────────────────────────────────────────────────────────── */
#define configCPU_CLOCK_HZ              ( 16000000UL )  /* 16 MHz default */
#define configTICK_RATE_HZ              ( (TickType_t) 1000 )

/* ── Scheduler ─────────────────────────────────────────────────────────── */
#define configUSE_PREEMPTION            1
#define configUSE_TIME_SLICING          1
#define configMAX_PRIORITIES            ( 8 )
#define configMINIMAL_STACK_SIZE        ( (unsigned short) 128 )
#define configTOTAL_HEAP_SIZE           ( (size_t) (14 * 1024) )
#define configMAX_TASK_NAME_LEN         ( 12 )
#define configUSE_16_BIT_TICKS          0
#define configIDLE_SHOULD_YIELD         1

/* ── Hooks ─────────────────────────────────────────────────────────────── */
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configUSE_MALLOC_FAILED_HOOK    1
#define configCHECK_FOR_STACK_OVERFLOW  2

/* ── Semaphore / Mutex / Queue ─────────────────────────────────────────── */
#define configUSE_MUTEXES               1
#define configUSE_COUNTING_SEMAPHORES   0
#define configUSE_QUEUE_SETS            0
#define configQUEUE_REGISTRY_SIZE       0

/* ── Software timers ───────────────────────────────────────────────────── */
#define configUSE_TIMERS                0
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH        5
#define configTIMER_TASK_STACK_DEPTH    ( configMINIMAL_STACK_SIZE )

/* ── Co-routines ───────────────────────────────────────────────────────── */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

/* ── API includes ──────────────────────────────────────────────────────── */
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelete             0
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_uxTaskPriorityGet       0
#define INCLUDE_vTaskPrioritySet        0

/* ── ARM Cortex-M4 interrupt priorities ────────────────────────────────── */
/* TM4C123 uses 3 priority bits => priority values 0–7                     */
#define configPRIO_BITS                 3
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      0x07
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  0x05

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* ── Assert ────────────────────────────────────────────────────────────── */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }

#endif /* FREERTOS_CONFIG_H */
