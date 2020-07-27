/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <misc/printk.h>
#include <stdio.h>
#include <string.h>
#include "degu_routing.h"
#include <openthread/udp.h>
#include <openthread/message.h>
#include <openthread/instance.h>
#include "utils/code_utils.h"

#define RECEV_MCAST_PORT 60888
#define SEND_MCAST_PORT 50888
static const char UDP_MCAST[] = "ff03::1";
static const char UDP_MESG[] = "degu::mcast";
static otUdpSocket sUdpSocket;

void initUDP_route(otInstance *aInstance)
{
        otSockAddr  listenSockAddr;

        memset(&sUdpSocket, 0, sizeof(sUdpSocket));
        memset(&listenSockAddr, 0, sizeof(listenSockAddr));

        listenSockAddr.mPort    = RECEV_MCAST_PORT;

        otUdpOpen(aInstance, &sUdpSocket, UDPmessageHandler, aInstance);
        otUdpBind(&sUdpSocket, &listenSockAddr);
}

void sendUDP_route(otInstance *aInstance)
{
       otError         error;
       otMessage *     message = NULL;
       otMessageInfo   messageInfo;
       otIp6Address    destAddr;
       memset(&messageInfo, 0, sizeof(messageInfo));

       otIp6AddressFromString(UDP_MCAST, &destAddr);
       messageInfo.mPeerAddr    = destAddr;
       messageInfo.mPeerPort    = SEND_MCAST_PORT;
        messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

       message = otUdpNewMessage(aInstance, NULL);
       otEXPECT_ACTION(message != NULL, error = OT_ERROR_NO_BUFS);
       error = otMessageAppend(message, UDP_MESG, sizeof(UDP_MESG));
       otEXPECT(error == OT_ERROR_NONE);
       error = otUdpSend(&sUdpSocket, message, &messageInfo);
 exit:
       if (error != OT_ERROR_NONE && message != NULL)
       {
               otMessageFree(message);
       }
}
