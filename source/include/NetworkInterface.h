/*
 * FreeRTOS+TCP <DEVELOPMENT BRANCH>
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

#include "FreeRTOS_IP.h"

/* INTERNAL API FUNCTIONS. */

/* Since there are multiple interfaces, there are multiple versions
 * of the following functions.
 * These are now declared static in NetworkInterface.c and their addresses
 * are stored in a struct NetworkInterfaceDescriptor_t.
 *
 *  BaseType_t xNetworkInterfaceInitialise( struct xNetworkInterface *pxInterface );
 *  BaseType_t xGetPhyLinkStatus( struct xNetworkInterface *pxInterface );
 */

/* The following function is defined only when BufferAllocation_1.c is linked in the project. */
size_t uxNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] );

BaseType_t xGetPhyLinkStatus( struct xNetworkInterface * pxInterface );

#define MAC_IS_MULTICAST( pucMACAddressBytes )    ( ( pucMACAddressBytes[ 0 ] & 1U ) != 0U )
#define MAC_IS_UNICAST( pucMACAddressBytes )      ( ( pucMACAddressBytes[ 0 ] & 1U ) == 0U )
#if ( ipconfigENABLE_SPECAL_VLAN_PORT_TAGGING != 0 )

/* If a switch is used as the PHY, some devices offer special VLAN tags that tag frames for their ingress or egress port.
 * When those thags are available and supported, the stack needs definitions for specifying these ports. */
    enum ENUM_SWITCH_PORTS
    {
        ETHERNET_PORT_CPU = 0,
        ETHERNET_PORT_0 = 0,
        ETHERNET_PORT_1,
        ETHERNET_PORT_2,

        ETHERNET_PORT_AUTO = 125,
        ETHERNET_PORT_ALL = 126,
        ETHERNET_PORT_NONE = 127,
    };
    typedef enum ENUM_SWITCH_PORTS eSwitchPorts_t;
#endif /* ( ipconfigENABLE_SPECAL_VLAN_PORT_TAGGING != 0) */


/* *INDENT-OFF* */
#ifdef __cplusplus
    } /* extern "C" */
#endif
/* *INDENT-ON* */

#endif /* NETWORK_INTERFACE_H */
