/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* https://www.renesas.com/en/document/oth/disclaimer8
*
* Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : NetworkInterface.c
* Device(s)    : RX
* Description  : Interfaces FreeRTOS TCP/IP stack to RX Ethernet driver.
***********************************************************************************************************************/

/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 07.03.2018 0.1     Development
***********************************************************************************************************************/

/***********************************************************************************************************************
*  Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"
/*#include "FreeRTOS_DNS.h" */
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"

#include "r_ether_rx_if.h"
#include "r_pinset.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ETHER_BUFSIZE_MIN    60

#if defined( BSP_MCU_RX65N ) || defined( BSP_MCU_RX64M ) || defined( BSP_MCU_RX71M ) || defined( BSP_MCU_RX72M ) || defined( BSP_MCU_RX72N )
    #if ETHER_CFG_MODE_SEL == 0
        #define R_ETHER_PinSet_CHANNEL_0()    R_ETHER_PinSet_ETHERC0_MII()
    #elif ETHER_CFG_MODE_SEL == 1
        #define R_ETHER_PinSet_CHANNEL_0()    R_ETHER_PinSet_ETHERC0_RMII()
    #endif
#elif defined( BSP_MCU_RX63N )
    #if ETHER_CFG_MODE_SEL == 0
        #define R_ETHER_PinSet_CHANNEL_0()    R_ETHER_PinSet_ETHERC_MII()
    #elif ETHER_CFG_MODE_SEL == 1
        #define R_ETHER_PinSet_CHANNEL_0()    R_ETHER_PinSet_ETHERC_RMII()
    #endif
#endif /* if defined( BSP_MCU_RX65N ) || defined( BSP_MCU_RX64M ) || defined( BSP_MCU_RX71M ) */

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
typedef enum
{
    eMACInit,   /* Must initialise MAC. */
    eMACPass,   /* Initialisation was successful. */
    eMACFailed, /* Initialisation failed. */
} eMAC_INIT_STATUS_TYPE;

static TaskHandle_t ether_receive_check_task_handle = NULL;
static TaskHandle_t xTaskToNotify = NULL;
static BaseType_t xPHYLinkStatus;
static BaseType_t xReportedStatus;
static eMAC_INIT_STATUS_TYPE xMacInitStatus = eMACInit;

/* Pointer to the interface object of this NIC */
static NetworkInterface_t * pxMyInterface = NULL;

static int16_t SendData( uint8_t * pucBuffer,
                         size_t length );
static int InitializeNetwork( void );
static void prvEMACDeferredInterruptHandlerTask( void * pvParameters );
static void clear_all_ether_rx_discriptors( uint32_t event );

int32_t callback_ether_regist( void );
void EINT_Trig_isr( void * );
void get_random_number( uint8_t * data,
                        uint32_t len );

void prvLinkStatusChange( BaseType_t xStatus );

/*-----------------------------------------------------------*/

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE != 0 )
    NetworkInterface_t * pxRX_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                       NetworkInterface_t * pxInterface );
#endif

/* Function to initialise the network interface */
BaseType_t xRX_NetworkInterfaceInitialise( NetworkInterface_t * pxInterface );

BaseType_t xRX_NetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                       NetworkBufferDescriptor_t * const pxDescriptor,
                                       BaseType_t bReleaseAfterSend );

static inline BaseType_t xRX_PHYGetLinkStatus( NetworkInterface_t * pxInterface );

NetworkInterface_t * pxRX_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                   NetworkInterface_t * pxInterface )
{
    static char pcName[ 17 ];

    /* This function pxRX_FillInterfaceDescriptor() adds a network-interface.
     * Make sure that the object pointed to by 'pxInterface'
     * is declared static or global, and that it will remain to exist. */

    snprintf( pcName, sizeof( pcName ), "eth%u", ( unsigned ) xEMACIndex );

    memset( pxInterface, '\0', sizeof( *pxInterface ) );
    pxInterface->pcName = pcName;                    /* Just for logging, debugging. */
    pxInterface->pvArgument = ( void * ) xEMACIndex; /* Has only meaning for the driver functions. */
    pxInterface->pfInitialise = xRX_NetworkInterfaceInitialise;
    pxInterface->pfOutput = xRX_NetworkInterfaceOutput;
    pxInterface->pfGetPhyLinkStatus = xRX_PHYGetLinkStatus;

    FreeRTOS_AddNetworkInterface( pxInterface );

    return pxInterface;
}

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE != 0 )
    NetworkInterface_t * pxFillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                    NetworkInterface_t * pxInterface )
    {
        return pxRX_FillInterfaceDescriptor( xEMACIndex, pxInterface );
    }
#endif

/***********************************************************************************************************************
 * Function Name: xRX_NetworkInterfaceInitialise ()
 * Description  : Initialization of Ethernet driver.
 * Arguments    : Pointer to the interface desc
 * Return Value : pdPASS, pdFAIL
 **********************************************************************************************************************/
BaseType_t xRX_NetworkInterfaceInitialise( NetworkInterface_t * pxInterface )
{
    BaseType_t xReturn;

    if( xMacInitStatus == eMACInit )
    {
        pxMyInterface = pxInterface;

        /*
         * Perform the hardware specific network initialization here using the Ethernet driver library to initialize the
         * Ethernet hardware, initialize DMA descriptors, and perform a PHY auto-negotiation to obtain a network link.
         *
         * InitialiseNetwork() uses Ethernet peripheral driver library function, and returns 0 if the initialization fails.
         */
        if( InitializeNetwork() == pdFALSE )
        {
            xMacInitStatus = eMACFailed;
        }
        else
        {
            /* Indicate that the MAC initialisation succeeded. */
            xMacInitStatus = eMACPass;
        }

        FreeRTOS_printf( ( "InitializeNetwork returns %s\n", ( xMacInitStatus == eMACPass ) ? "OK" : " Fail" ) );
    }

    if( xMacInitStatus == eMACPass )
    {
        xReturn = xPHYLinkStatus;
    }
    else
    {
        xReturn = pdFAIL;
    }

    FreeRTOS_printf( ( "xNetworkInterfaceInitialise returns %d\n", xReturn ) );

    return xReturn;
} /* End of function xNetworkInterfaceInitialise() */


/***********************************************************************************************************************
 * Function Name: xRX_NetworkInterfaceOutput ()
 * Description  : Simple network output interface.
 * Arguments    : pxInterface, pxDescriptor, xReleaseAfterSend
 * Return Value : pdTRUE, pdFALSE
 **********************************************************************************************************************/
BaseType_t xRX_NetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                       NetworkBufferDescriptor_t * const pxDescriptor,
                                       BaseType_t xReleaseAfterSend )
{
    BaseType_t xReturn = pdFALSE;

    /* As there is only a single instance of the EMAC, there is only one pxInterface object. */
    ( void ) pxInterface;

    /* Simple network interfaces (as opposed to more efficient zero copy network
     * interfaces) just use Ethernet peripheral driver library functions to copy
     * data from the FreeRTOS+TCP buffer into the peripheral driver's own buffer.
     * This example assumes SendData() is a peripheral driver library function that
     * takes a pointer to the start of the data to be sent and the length of the
     * data to be sent as two separate parameters.  The start of the data is located
     * by pxDescriptor->pucEthernetBuffer.  The length of the data is located
     * by pxDescriptor->xDataLength. */
    if( xPHYLinkStatus != 0 )
    {
        if( SendData( pxDescriptor->pucEthernetBuffer, pxDescriptor->xDataLength ) >= 0 )
        {
            xReturn = pdTRUE;
            /* Call the standard trace macro to log the send event. */
            iptraceNETWORK_INTERFACE_TRANSMIT();
        }
    }
    else
    {
        /* As the PHY Link Status is low, it makes no sense trying to deliver a packet. */
    }

    if( xReleaseAfterSend != pdFALSE )
    {
        /* It is assumed SendData() copies the data out of the FreeRTOS+TCP Ethernet
         * buffer.  The Ethernet buffer is therefore no longer needed, and must be
         * freed for re-use. */
        vReleaseNetworkBufferAndDescriptor( pxDescriptor );
    }

    return xReturn;
} /* End of function xNetworkInterfaceOutput() */


/***********************************************************************************************************************
 * Function Name: prvEMACDeferredInterruptHandlerTask ()
 * Description  : The deferred interrupt handler is a standard RTOS task.
 * Arguments    : pvParameters
 * Return Value : none
 **********************************************************************************************************************/
static void prvEMACDeferredInterruptHandlerTask( void * pvParameters )
{
    NetworkBufferDescriptor_t * pxBufferDescriptor;
    int32_t xBytesReceived = 0;

    /* Avoid compiler warning about unreferenced parameter. */
    ( void ) pvParameters;

    /* Used to indicate that xSendEventStructToIPTask() is being called because
     * of an Ethernet receive event. */
    IPStackEvent_t xRxEvent;

    uint8_t * buffer_pointer;

    /* Some variables related to monitoring the PHY. */
    TimeOut_t xPhyTime;
    TickType_t xPhyRemTime;
    const TickType_t ulMaxBlockTime = pdMS_TO_TICKS( 100UL );

    vTaskSetTimeOutState( &xPhyTime );
    xPhyRemTime = pdMS_TO_TICKS( ipconfigPHY_LS_LOW_CHECK_TIME_MS );

    FreeRTOS_printf( ( "Deferred Interrupt Handler Task started\n" ) );
    xTaskToNotify = ether_receive_check_task_handle;

    for( ; ; )
    {
        #if ( ipconfigHAS_PRINTF != 0 )
        {
            /* Call a function that monitors resources: the amount of free network
             * buffers and the amount of free space on the heap.  See FreeRTOS_IP.c
             * for more detailed comments. */
            vPrintResourceStats();
        }
        #endif /* ( ipconfigHAS_PRINTF != 0 ) */

        /* Wait for the Ethernet MAC interrupt to indicate that another packet
         * has been received.  */
        if( xBytesReceived <= 0 )
        {
            ulTaskNotifyTake( pdFALSE, ulMaxBlockTime );
        }

        /* See how much data was received.  */
        xBytesReceived = R_ETHER_Read_ZC2( ETHER_CHANNEL_0, ( void ** ) &buffer_pointer );

        if( xBytesReceived < 0 )
        {
            /* This is an error. Logged. */
            FreeRTOS_debug_printf( ( "R_ETHER_Read_ZC2: rc = %d\n", xBytesReceived ) );
        }
        else if( xBytesReceived > 0 )
        {
            /* Allocate a network buffer descriptor that points to a buffer
             * large enough to hold the received frame.  As this is the simple
             * rather than efficient example the received data will just be copied
             * into this buffer. */
            pxBufferDescriptor = pxGetNetworkBufferWithDescriptor( ( size_t ) xBytesReceived, 0 );

            if( pxBufferDescriptor != NULL )
            {
                /* pxBufferDescriptor->pucEthernetBuffer now points to an Ethernet
                 * buffer large enough to hold the received data.  Copy the
                 * received data into pcNetworkBuffer->pucEthernetBuffer.  Here it
                 * is assumed ReceiveData() is a peripheral driver function that
                 * copies the received data into a buffer passed in as the function's
                 * parameter.  Remember! While is is a simple robust technique -
                 * it is not efficient.  An example that uses a zero copy technique
                 * is provided further down this page. */
                memcpy( pxBufferDescriptor->pucEthernetBuffer, buffer_pointer, ( size_t ) xBytesReceived );
                /*ReceiveData( pxBufferDescriptor->pucEthernetBuffer ); */

                /* Set the actual packet length, in case a larger buffer was returned. */
                pxBufferDescriptor->xDataLength = ( size_t ) xBytesReceived;
                pxBufferDescriptor->pxInterface = pxMyInterface;
                pxBufferDescriptor->pxEndPoint = FreeRTOS_MatchingEndpoint( pxMyInterface, pxBufferDescriptor->pucEthernetBuffer );

                R_ETHER_Read_ZC2_BufRelease( ETHER_CHANNEL_0 );

                /* See if the data contained in the received Ethernet frame needs
                * to be processed.  NOTE! It is preferable to do this in
                * the interrupt service routine itself, which would remove the need
                * to unblock this task for packets that don't need processing. */
                if( ( eConsiderFrameForProcessing( pxBufferDescriptor->pucEthernetBuffer ) == eProcessBuffer ) && ( pxBufferDescriptor->pxEndPoint != NULL ) )
                {
                    /* The event about to be sent to the TCP/IP is an Rx event. */
                    xRxEvent.eEventType = eNetworkRxEvent;

                    /* pvData is used to point to the network buffer descriptor that
                     * now references the received data. */
                    xRxEvent.pvData = ( void * ) pxBufferDescriptor;

                    /* Send the data to the TCP/IP stack. */
                    if( xSendEventStructToIPTask( &xRxEvent, 0 ) == pdFALSE )
                    {
                        /* The buffer could not be sent to the IP task so the buffer must be released. */
                        vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );

                        /* Make a call to the standard trace macro to log the occurrence. */
                        iptraceETHERNET_RX_EVENT_LOST();
                        clear_all_ether_rx_discriptors( 0 );
                    }
                    else
                    {
                        /* The message was successfully sent to the TCP/IP stack.
                        * Call the standard trace macro to log the occurrence. */
                        iptraceNETWORK_INTERFACE_RECEIVE();
                        R_BSP_NOP();
                    }
                }
                else
                {
                    /* The Ethernet frame can be dropped, but the Ethernet buffer must be released. */
                    vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );
                }
            }
            else
            {
                /* The event was lost because a network buffer was not available.
                 * Call the standard trace macro to log the occurrence. */
                iptraceETHERNET_RX_EVENT_LOST();
                clear_all_ether_rx_discriptors( 1 );
                FreeRTOS_printf( ( "R_ETHER_Read_ZC2: Cleared descriptors\n" ) );
            }
        }

        if( xBytesReceived > 0 )
        {
            /* A packet was received. No need to check for the PHY status now,
             * but set a timer to check it later on. */
            vTaskSetTimeOutState( &xPhyTime );
            xPhyRemTime = pdMS_TO_TICKS( ipconfigPHY_LS_HIGH_CHECK_TIME_MS );

            /* Indicate that the Link Status is high, so that
             * xNetworkInterfaceOutput() can send packets. */
            if( xPHYLinkStatus == 0 )
            {
                xPHYLinkStatus = 1;
                FreeRTOS_printf( ( "prvEMACHandlerTask: PHY LS assume %d\n", xPHYLinkStatus ) );
            }
        }
        else if( ( xTaskCheckForTimeOut( &xPhyTime, &xPhyRemTime ) != pdFALSE ) || ( FreeRTOS_IsNetworkUp() == pdFALSE ) )
        {
            R_ETHER_LinkProcess( ETHER_CHANNEL_0 );

            if( xPHYLinkStatus != xReportedStatus )
            {
                xPHYLinkStatus = xReportedStatus;
                FreeRTOS_printf( ( "prvEMACHandlerTask: PHY LS now %d\n", xPHYLinkStatus ) );
            }

            vTaskSetTimeOutState( &xPhyTime );

            if( xPHYLinkStatus != 0 )
            {
                xPhyRemTime = pdMS_TO_TICKS( ipconfigPHY_LS_HIGH_CHECK_TIME_MS );
            }
            else
            {
                xPhyRemTime = pdMS_TO_TICKS( ipconfigPHY_LS_LOW_CHECK_TIME_MS );
            }
        }
    }
} /* End of function prvEMACDeferredInterruptHandlerTask() */


/***********************************************************************************************************************
 * Function Name: uxNetworkInterfaceAllocateRAMToBuffers ()
 * Description  : .
 * Arguments    : pxNetworkBuffers
 * Return Value : none
 **********************************************************************************************************************/

size_t uxNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
{
    uint32_t ul;
    uint8_t * buffer_address;
    portPOINTER_SIZE_TYPE uxStartAddress;

    static uint8_t ETH_BUFFERS[ ( ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS * ETHER_CFG_BUFSIZE ) + portBYTE_ALIGNMENT ];

    /* Align the buffer start address to portBYTE_ALIGNMENT bytes */
    uxStartAddress = ( portPOINTER_SIZE_TYPE ) & ETH_BUFFERS[ 0 ];
    uxStartAddress += portBYTE_ALIGNMENT;
    uxStartAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );

    buffer_address = ( uint8_t * ) uxStartAddress;

    for( ul = 0; ul < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; ul++ )
    {
        pxNetworkBuffers[ ul ].pucEthernetBuffer = buffer_address + ipBUFFER_PADDING;
        *( ( unsigned * ) buffer_address ) = ( unsigned ) ( &( pxNetworkBuffers[ ul ] ) );
        buffer_address += ETHER_CFG_BUFSIZE;
    }

    return( ETHER_CFG_BUFSIZE - ipBUFFER_PADDING );
} /* End of function uxNetworkInterfaceAllocateRAMToBuffers() */

/***********************************************************************************************************************
 * Function Name: prvLinkStatusChange ()
 * Description  : Function will be called when the Link Status of the phy has changed ( see ether_callback.c )
 * Arguments    : xStatus : true when status has become high
 * Return Value : void
 **********************************************************************************************************************/
void prvLinkStatusChange( BaseType_t xStatus )
{
    if( xReportedStatus != xStatus )
    {
        FreeRTOS_printf( ( "prvLinkStatusChange( %d )\n", xStatus ) );
        xReportedStatus = xStatus;
    }
}

/***********************************************************************************************************************
 * Function Name: InitializeNetwork ()
 * Description  :
 * Arguments    : none
 * Return Value : pdTRUE, pdFALSE
 **********************************************************************************************************************/
static int InitializeNetwork( void )
{
    ether_return_t eth_ret;
    BaseType_t return_code = pdFALSE;
    ether_param_t param;

    /* Read the mac address after it has been initialized by the FreeRTOS IP Stack, rather than from defines
     * as the mac address is usually read from the EEPROM, and it might be different to the mac address in
     * the defines, especially in production environments
     */
    configASSERT( pxMyInterface );
    const uint8_t * myethaddr = &pxMyInterface->pxEndPoint->xMACAddress.ucBytes[ 0 ];

    R_ETHER_PinSet_CHANNEL_0();
    R_ETHER_Initial();
    callback_ether_regist();

    param.channel = ETHER_CHANNEL_0;
    eth_ret = R_ETHER_Control( CONTROL_POWER_ON, param ); /* PHY mode settings, module stop cancellation */

    if( ETHER_SUCCESS != eth_ret )
    {
        return pdFALSE;
    }

    eth_ret = R_ETHER_Open_ZC2( ETHER_CHANNEL_0, myethaddr, ETHER_FLAG_OFF );

    if( ETHER_SUCCESS != eth_ret )
    {
        return pdFALSE;
    }

    if( ether_receive_check_task_handle == NULL )
    {
        return_code = xTaskCreate( prvEMACDeferredInterruptHandlerTask,
                                   "ETHER_RECEIVE_CHECK_TASK",
                                   512u,
                                   0,
                                   configMAX_PRIORITIES - 1,
                                   &ether_receive_check_task_handle );
    }
    else
    {
        return_code = pdTRUE;
    }

    if( pdFALSE == return_code )
    {
        return pdFALSE;
    }

    return pdTRUE;
} /* End of function InitializeNetwork() */


/***********************************************************************************************************************
 * Function Name: SendData ()
 * Description  :
 * Arguments    : pucBuffer, length
 * Return Value : 0 success, negative fail
 **********************************************************************************************************************/
static int16_t SendData( uint8_t * pucBuffer,
                         size_t length )
{
    ether_return_t ret;
    uint8_t * pwrite_buffer;
    uint16_t write_buf_size;

    /* (1) Retrieve the transmit buffer location controlled by the  descriptor. */
    ret = R_ETHER_Write_ZC2_GetBuf( ETHER_CHANNEL_0, ( void ** ) &pwrite_buffer, &write_buf_size );

    if( ETHER_SUCCESS == ret )
    {
        if( write_buf_size >= length )
        {
            memcpy( pwrite_buffer, pucBuffer, length );
        }

        if( length < ETHER_BUFSIZE_MIN )                                             /*under minimum*/
        {
            memset( ( pwrite_buffer + length ), 0, ( ETHER_BUFSIZE_MIN - length ) ); /*padding*/
            length = ETHER_BUFSIZE_MIN;                                              /*resize*/
        }

        ret = R_ETHER_Write_ZC2_SetBuf( ETHER_CHANNEL_0, ( uint16_t ) length );
        ret = R_ETHER_CheckWrite( ETHER_CHANNEL_0 );
    }

    if( ETHER_SUCCESS != ret )
    {
        return -5; /* XXX return meaningful value */
    }
    else
    {
        return 0;
    }
} /* End of function SendData() */


/***********************************************************************************************************************
* Function Name: EINT_Trig_isr
* Description  : Standard frame received interrupt handler
* Arguments    : ectrl - EDMAC and ETHERC control structure
* Return Value : None
* Note         : This callback function is executed when EINT0 interrupt occurred.
***********************************************************************************************************************/
void EINT_Trig_isr( void * ectrl )
{
    ether_cb_arg_t * pdecode;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    pdecode = ( ether_cb_arg_t * ) ectrl;

    if( pdecode->status_eesr & 0x00040000 ) /* EDMAC FR (Frame Receive Event) interrupt */
    {
        if( xTaskToNotify != NULL )
        {
            vTaskNotifyGiveFromISR( ether_receive_check_task_handle, &xHigherPriorityTaskWoken );
        }

        /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
         * should be performed to ensure the interrupt returns directly to the highest
         * priority task.  The macro used for this purpose is dependent on the port in
         * use and may be called portEND_SWITCHING_ISR(). */
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        /* Complete interrupt handler for other events. */
    }
} /* End of function EINT_Trig_isr() */


static void clear_all_ether_rx_discriptors( uint32_t event )
{
    int32_t xBytesReceived;
    uint8_t * buffer_pointer;

    /* Avoid compiler warning about unreferenced parameter. */
    ( void ) event;

    while( 1 )
    {
        /* See how much data was received.  */
        xBytesReceived = R_ETHER_Read_ZC2( ETHER_CHANNEL_0, ( void ** ) &buffer_pointer );

        if( 0 > xBytesReceived )
        {
            /* This is an error. Ignored. */
        }
        else if( 0 < xBytesReceived )
        {
            R_ETHER_Read_ZC2_BufRelease( ETHER_CHANNEL_0 );
            iptraceETHERNET_RX_EVENT_LOST();
        }
        else
        {
            break;
        }
    }
}

static inline BaseType_t xRX_PHYGetLinkStatus( NetworkInterface_t * pxInterface )
{
    ( void ) pxInterface;
    return( xPHYLinkStatus != 0 );
}


/***********************************************************************************************************************
 * End of file "NetworkInterface.c"
 **********************************************************************************************************************/
