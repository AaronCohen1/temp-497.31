#include <stdio.h>

/* FreeRTOS.org includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo includes. */
#include "basic_io.h"

/* Project includes. */
#include "main.h"
//#include "dma.h"
#include "dac.h"
#include "tonegen.h"
#include "testbench_task.h"
#include "dtmf_detect_task.h"
#include "dtmf_data.h"
#include "adc_task.h"
#include "ui.h"
#include "keypad.h"
#include "uart.h"

/*#define _DTMF_STANDALONE */
/*#define TONEGEN_INPUT_UNIT_TEST */

/*-----------------------------------------------------------*/

static QueueHandle_t sampQ;
static QueueHandle_t resultQ;
static struct TestBenchTaskParam_t TestBenchTaskParam;
static struct DTMFDetectTaskParam_t DTMFDetectTaskParam;
DTMFSampleType ADC_BUFFERS[NUM_ADC_BUFFERS][DTMFSampleSize];
static xQueueType lQueues;
/* Queue Into ToneGenerator Task */
xQueueHandle xQueueToneInput;

/* Queues Between ToneGenerator and DACHandler Task */
xQueueHandle xQueueDMARequest;
xQueueHandle dacResponseHandle;


void vProcessTask( void *pvParameters );

int main( void )
{
	// Init the semi-hosting.
	printf( "\n" );

	// DAC/DMA Setup
	NVIC_DisableIRQ( DMA_IRQn);
	InitializeDAC();
	//InitializeADC();
	InitializeDMA();

	/* Instantiate queue and semaphores */
	xQueueToneInput = xQueueCreate( DTMF_REQ_QUEUE_SIZE, sizeof( char ) );
	xQueueDMARequest = xQueueCreate( DMA_REQ_QUEUE_SIZE, sizeof( DAC_Setup_Message ));
	dacResponseHandle = xQueueCreate( DMA_COMP_QUEUE_SIZE, sizeof( uint32_t ) );
	sampQ = xQueueCreate( 1, sizeof(DTMFSampleType *) );
	resultQ = xQueueCreate( 1, sizeof(struct DTMFResult_t) );
	lQueues.xIoInputQueue = xQueueCreate( 2, sizeof(xData) );
	lQueues.xDACQueue =     xQueueCreate( 2, sizeof(xData) );


	if( sampQ != NULL &&
		resultQ != NULL &&
		xQueueToneInput != NULL &&
		xQueueDMARequest != NULL &&
		dacResponseHandle != NULL &&
		lQueues.xIoInputQueue != NULL &&
		lQueues.xDACQueue != NULL) {

	  xTaskCreate(  vTaskToneGenerator, /* Pointer to the function that implements the task. */
					  "ToneGenerator",          /* Text name for the task.  This is to facilitate debugging only. */
					  240,                      /* Stack depth in words. */
					  NULL,                     /* No input data */
					  1,                        /* This task will run at priority 1. */
					  NULL );                   /* We are not using the task handle. */

	  #ifdef TONEGEN_INPUT_UNIT_TEST
	    xTaskCreate( vTaskToneRequestTest, "ToneRequestTest", 240, NULL, configMAX_PRIORITIES-2/*4*/, NULL );
	  #endif

	  #ifdef  TONEGEN_DMA_UNIT_TEST
	    xTaskCreate( vTaskDMAHandlerTest, "DMAHandlerTest", 240, NULL, 2, NULL );
	  #else
	    //============================================================================
	    // Create DAC and DMA Tasks
	    //============================================================================
	    xTaskCreate(  DAC_Handler,/* Pointer to the function that implements the task. */
						"DAC",            /* Text name for the task.  This is to facilitate debugging only. */
						240,              /* Stack depth in words. */
						(void*)xQueueDMARequest, /* Pass the text to be printed in as the task parameter. */
						configMAX_PRIORITIES-1,           /* This task will run at priority 1. */
						NULL );           /* We are not using the task handle. */

	    #endif

		xTaskCreate(	vAdcTask,
						"tADC",
						240,
						(void *)sampQ,
						2,
						NULL );

#ifdef __DTMF_PERF__
		TestBenchTaskParam.sampQ = sampQ;
		TestBenchTaskParam.resultQ = resultQ;
		xTaskCreate(	vTestBenchTask,
						"tTB",
						500,
						(void *)&TestBenchTaskParam,
						3,
						NULL );
#endif

		DTMFDetectTaskParam.sampQ = sampQ;
		DTMFDetectTaskParam.resultQ = resultQ;
		xTaskCreate(	vDTMFDetectTask,
						"tDetect",
						500,
						(void *)&DTMFDetectTaskParam,
						configMAX_PRIORITIES-3,
						NULL );

		// Add this when UART task code is ready
		// xTaskCreate( UARTInterfaceTask, "UART_Task", 240, &(lQueues.xDACQueue), 1, NULL );
		xTaskCreate( uart_tx_handler, "Tx Task", 500, NULL, 2, NULL );
		xTaskCreate( uart_rx_handler, "Rx Task", 500, NULL, 2, NULL );
		uart_configure(UART_PARITY_NONE,UART_1_STOP,UART_8_BIT);

		/* Create four instances of the task that will write to the queue */
		xTaskCreate( gpioInterfaceTask, "Keypad_Task", 240, &(lQueues.xIoInputQueue), 1, NULL);
		xTaskCreate( uiInterfaceTask, "UI_Task", 240, &lQueues, 2, NULL );

		/* Start the scheduler so our tasks start executing. */
		vTaskStartScheduler();
	}

	/* If all is well we will never reach here as the scheduler will now be
	running.  If we do reach here then it is likely that there was insufficient
	heap available for the idle task to be created. */
	for( ;; );
	return 0;
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* This function will only be called if an API call to create a task, queue
	or semaphore fails because there is too little heap RAM remaining. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed char *pcTaskName )
{
	/* This function will only be called if a task overflows its stack.  Note
	that stack overflow checking does slow down the context switch
	implementation. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* This example does not use the idle hook to perform any processing. */
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This example does not use the tick hook to perform any processing. */
}
