/*
 * FreeRTOS memory safety proofs with CBMC.
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "queue.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_TCP_State_Handling.h"

/* CBMC includes. */
#include "cbmc.h"
#include "../../utility/memory_assignments.c"

/* Abstraction of uxIPHeaderSizePacket. This test case only uses to test IPv4 path. */
size_t uxIPHeaderSizePacket( const NetworkBufferDescriptor_t * pxNetworkBuffer )
{
    return ipSIZE_OF_IPv4_HEADER;
}

/* Abstraction of FreeRTOS_socket. Return NULL or valid socket handler. */
Socket_t FreeRTOS_socket( BaseType_t xDomain,
                          BaseType_t xType,
                          BaseType_t xProtocol )
{
    return ensure_FreeRTOS_Socket_t_is_allocated();
}

BaseType_t vSocketBind( FreeRTOS_Socket_t * pxSocket,
                        struct freertos_sockaddr * pxBindAddress,
                        size_t uxAddressLength,
                        BaseType_t xInternal )
{
    BaseType_t xRet;

    __CPROVER_assert( pxSocket != NULL,
                      "FreeRTOS precondition: pxSocket != NULL" );
    __CPROVER_assert( pxBindAddress != NULL,
                      "FreeRTOS precondition: pxBindAddress != NULL" );

    return xRet;
}

void * vSocketClose( FreeRTOS_Socket_t * pxSocket )
{
    __CPROVER_assert( pxSocket != NULL, "Closing socket cannot be NULL" );

    return NULL;
}

BaseType_t prvTCPSendReset( NetworkBufferDescriptor_t * pxNetworkBuffer )
{
    BaseType_t xReturn;

    __CPROVER_assume( ( xReturn == pdTRUE ) || ( xReturn == pdFALSE ) );

    return xReturn;
}

void vTCPStateChange( FreeRTOS_Socket_t * pxSocket,
                      enum eTCP_STATE eTCPState )
{
}

BaseType_t prvTCPCreateWindow( FreeRTOS_Socket_t * pxSocket )
{
    BaseType_t xReturn;

    __CPROVER_assume( ( xReturn == pdTRUE ) || ( xReturn == pdFALSE ) );

    return xReturn;
}

void prvSocketSetMSS( FreeRTOS_Socket_t * pxSocket )
{
    __CPROVER_assert( pxSocket != NULL, "pxSocket cannot be NULL" );
}

size_t FreeRTOS_GetLocalAddress( ConstSocket_t xSocket,
                                 struct freertos_sockaddr * pxAddress )
{
    size_t uxReturn;

    __CPROVER_assert( pxAddress != NULL, "pxAddress cannot be NULL" );

    return uxReturn;
}

void harness()
{
    size_t xDataLength;
    FreeRTOS_Socket_t * pxSocket = ensure_FreeRTOS_Socket_t_is_allocated();
    NetworkBufferDescriptor_t * pxNetworkBuffer = ( NetworkBufferDescriptor_t * ) safeMalloc( sizeof( FreeRTOS_Socket_t ) );

    /* The length of buffer must be larger than or equal to TCP minimum requirement. */
    __CPROVER_assume( xDataLength >= sizeof( TCPPacket_t ) && xDataLength <= ipconfigNETWORK_MTU );

    /* Socket is guaranteed to be non-NULL. */
    __CPROVER_assume( pxSocket != NULL );

    /* Network buffer is guaranteed to be non-NULL. */
    __CPROVER_assume( pxNetworkBuffer != NULL );

    pxNetworkBuffer->pucEthernetBuffer = ( uint8_t * ) safeMalloc( xDataLength );

    /* Network buffer is guaranteed to be non-NULL. */
    __CPROVER_assume( pxNetworkBuffer->pucEthernetBuffer != NULL );

    pxNetworkBuffer->xDataLength = xDataLength;
    pxNetworkBuffer->pxEndPoint = ( NetworkEndPoint_t * ) safeMalloc( sizeof( NetworkEndPoint_t ) );

    prvHandleListen( pxSocket, pxNetworkBuffer );
}
