/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <openthread/config.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openthread/coap.h>
#include <openthread/openthread.h>
#include <openthread/types.h>
#include <openthread/platform/logging.h>

#include <openthread/cli.h>
#include <openthread/ip6.h>
#include <openthread/link.h>

#include "common/encoding.hpp"

#define APP_RESPPAYLOAD_SIZE 200
#define APP_IPSTRING_SIZE 64

using ot::Encoding::BigEndian::HostSwap16;
using ot::Encoding::BigEndian::HostSwap32;

/**
 * Context information to be passed to our handlers.
 */
struct CoapUtilHandlerContext
{
    otInstance *aInstance;
};

/**
 * The instance of our context information structure.
 */
static struct CoapUtilHandlerContext utilContext = {.aInstance = NULL};

/**
 * Our default handler, this will display a page for all requests.
 */
static void defaultHandler(void *               aContext,
                           otCoapHeader *       aHeader,
                           otMessage *          aMessage,
                           const otMessageInfo *aMessageInfo);
/**
 * Handler for the Ping path.
 */
static void pingHandler(void *aContext, otCoapHeader *aHeader, otMessage *aMessage, const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the Ping path.
 */
static otCoapResource pingResource = {.mUriPath = "ping",
                                      .mHandler = &pingHandler,
                                      .mContext = (void *)&utilContext,
                                      .mNext    = NULL};
/**
 * Handler for the Identity path.
 */
static void identityHandler(void *               aContext,
                            otCoapHeader *       aHeader,
                            otMessage *          aMessage,
                            const otMessageInfo *aMessageInfo);
/**
 * Resource definition for the Identity path.
 */
static otCoapResource identityResource = {.mUriPath = "ident",
                                          .mHandler = &identityHandler,
                                          .mContext = (void *)&utilContext,
                                          .mNext    = NULL};

otError coapUtilInit(otInstance *aInstance)
{
    utilContext.aInstance = aInstance;
    otCoapSetDefaultHandler(aInstance, &defaultHandler, (void *)&utilContext);
    otCoapAddResource(aInstance, &pingResource);
    otCoapAddResource(aInstance, &identityResource);
    return otCoapStart(aInstance, OT_DEFAULT_COAP_PORT);
}

static void defaultHandler(void *               aContext,
                           otCoapHeader *       aHeader,
                           otMessage *          aMessage,
                           const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapUtilHandlerContext *handlerContext = (struct CoapUtilHandlerContext *)aContext;

    /*
     * The default handler.  We need to know:
     * - what type of message this is (Confirmable, Non-confirmable,
     *   Acknowlegement or Reset)
     * - what type of request this is (GET, PUT, POST, DELETE, … etc).
     *
     * For this simple demo, we only care about confirmable requests, as these
     * are what carry our HTTP requests.  We will reply with a so-called
     * "piggy-back" response by appending it to the ACK reply we send.
     */

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader))
    {
    case OT_COAP_CODE_GET: /* A GET request */
    {
        /*
         * In our case, we don't care about the message, we just
         * send a reply.  We need to copy the message ID and token
         * from the original message.  The reply is an ACK with
         * content, so we set the payload marker to indicate this.
         */
        otCoapHeader replyHeader;
        otMessage *  replyMessage;

        otCoapHeaderInit(&replyHeader, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CONTENT);

        otCoapHeaderSetToken(&replyHeader, otCoapHeaderGetToken(aHeader), otCoapHeaderGetTokenLength(aHeader));

        otCoapHeaderSetMessageId(&replyHeader, otCoapHeaderGetMessageId(aHeader));
        otCoapHeaderAppendContentFormatOption(&replyHeader, OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN);

        otCoapHeaderSetPayloadMarker(&replyHeader);

        replyMessage = otCoapNewMessage(handlerContext->aInstance, &replyHeader);

        if (replyMessage)
        {
            otError result;
            /*
             * Constructing the payload of the response, a simple string "Hello World"
             */
            result = otMessageAppend(replyMessage, "Hello World", 11);

            if (result == OT_ERROR_NONE)
            {
                /* All good, now send it */
                result = otCoapSendResponse(handlerContext->aInstance, replyMessage, aMessageInfo);
                otCliUartOutputFormat("Got COAP message in default handler, replying 'Hello World'");
            }

            if (result != OT_ERROR_NONE)
            {
                /* There was an issue above, free up the message */
                otMessageFree(replyMessage);
            }
        }
    }
    break;
    default:
        break;
    }
}

static void pingHandler(void *aContext, otCoapHeader *aHeader, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    /* Ignore message */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapUtilHandlerContext *handlerContext = (struct CoapUtilHandlerContext *)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }
    switch (otCoapHeaderGetCode(aHeader))
    {
    case OT_COAP_CODE_GET: /* A GET request */
    {
        /*
         * In our case, we don't care about the message, we just
         * send a reply.  We need to copy the message ID and token
         * from the original message, and set the payload marker.
         */
        otCoapHeader replyHeader;
        otMessage *  replyMessage;

        otCoapHeaderInit(&replyHeader, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CONTENT);

        otCoapHeaderSetToken(&replyHeader, otCoapHeaderGetToken(aHeader), otCoapHeaderGetTokenLength(aHeader));

        otCoapHeaderSetMessageId(&replyHeader, otCoapHeaderGetMessageId(aHeader));

        otCoapHeaderAppendContentFormatOption(&replyHeader, OT_COAP_OPTION_CONTENT_FORMAT_JSON);

        otCoapHeaderSetPayloadMarker(&replyHeader);

        replyMessage = otCoapNewMessage(handlerContext->aInstance, &replyHeader);

        if (replyMessage)
        {
            otError result;
            char    responseData[APP_RESPPAYLOAD_SIZE];
            /*
             * Constructing the payload of the response
             */
            int len = snprintf(responseData, sizeof(responseData) - 1, "%s", "{\"res\":\"pong\"}");
            result  = otMessageAppend(replyMessage, responseData, (uint16_t)len);

            if (result == OT_ERROR_NONE)
            {
                /* All good, now send it */
                result = otCoapSendResponse(handlerContext->aInstance, replyMessage, aMessageInfo);
                otCliUartOutputFormat("Got COAP message /ping, replying 'pong'");
            }

            if (result != OT_ERROR_NONE)
            {
                /* There was an issue above, free up the message */
                otMessageFree(replyMessage);
            }
        }
    }
    break;
    default:
        break;
    }
}

/**
 * Defining how to reply to a LEDs request… we do this here as we'll need
 * to do it a few times.
 */
static void identityReplyHandler(struct CoapUtilHandlerContext *handlerContext,
                                 otCoapHeader *                 aHeader,
                                 const otMessageInfo *          aMessageInfo)
{
    otCoapHeader replyHeader;
    otMessage *  replyMessage;

    /*
     * Reply is an ACK with content to come
     */
    otCoapHeaderInit(&replyHeader, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CONTENT);

    /*
     * Copy the token from the request header
     */
    otCoapHeaderSetToken(&replyHeader, otCoapHeaderGetToken(aHeader), otCoapHeaderGetTokenLength(aHeader));

    /*
     * Copy the message ID from the request header
     */
    otCoapHeaderSetMessageId(&replyHeader, otCoapHeaderGetMessageId(aHeader));

    /*
     * Set the Content Format option to JSON
     */
    otCoapHeaderAppendContentFormatOption(&replyHeader, OT_COAP_OPTION_CONTENT_FORMAT_JSON);

    /*
     * We're generating a piggy-back response, set the marker indicating this
     * or we'll get a PARSE error when we try to send.
     */
    otCoapHeaderSetPayloadMarker(&replyHeader);

    replyMessage = otCoapNewMessage(handlerContext->aInstance, &replyHeader);

    if (replyMessage)
    {
        otError      result;
        otExtAddress extAddress;
        /*
         * Getting the eui64 value and ipaddrs of the node using the OT API
         */
        otLinkGetFactoryAssignedIeeeEui64(handlerContext->aInstance, &extAddress);
        const otNetifAddress *addrs = otIp6GetUnicastAddresses(handlerContext->aInstance);

        char ippddrString[APP_IPSTRING_SIZE];
        ippddrString[0] = '\0';
        /*
         * Looping in all the node's ip addresses
         */
        for (const otNetifAddress *addr = addrs; addr; addr = addr->mNext)
        {
            if (addr->mScopeOverride == 3 && addr->mRloc == false)
            {
                /*
                 * This is the mesh local ipaddr of the node, converting a to string
                 */
                snprintf(ippddrString, sizeof(ippddrString) - 1, "%x:%x:%x:%x:%x:%x:%x:%x",
                         HostSwap16(addr->mAddress.mFields.m16[0]), HostSwap16(addr->mAddress.mFields.m16[1]),
                         HostSwap16(addr->mAddress.mFields.m16[2]), HostSwap16(addr->mAddress.mFields.m16[3]),
                         HostSwap16(addr->mAddress.mFields.m16[4]), HostSwap16(addr->mAddress.mFields.m16[5]),
                         HostSwap16(addr->mAddress.mFields.m16[6]), HostSwap16(addr->mAddress.mFields.m16[7]));
            }
        }

        /*
         * Constructing the payload of the response
         */
        char response[APP_RESPPAYLOAD_SIZE];
        int  len = snprintf(response, sizeof(response) - 1, "{\"eui\":\"%x:%x:%x:%x:%x:%x:%x:%x\",\"ipaddr\":\"%s\"}",
                           extAddress.m8[0], extAddress.m8[1], extAddress.m8[2], extAddress.m8[3], extAddress.m8[4],
                           extAddress.m8[5], extAddress.m8[6], extAddress.m8[7], ippddrString);
        result   = otMessageAppend(replyMessage, response, (uint16_t)len);

        if (result == OT_ERROR_NONE)
        {
            /* All good, now send it */
            result = otCoapSendResponse(handlerContext->aInstance, replyMessage, aMessageInfo);
            otCliUartOutputFormat("Got COAP message /ident, replying '%s'", response);
        }

        if (result != OT_ERROR_NONE)
        {
            /* There was an issue above, free up the message */
            otMessageFree(replyMessage);
        }
    }
}

static void identityHandler(void *               aContext,
                            otCoapHeader *       aHeader,
                            otMessage *          aMessage,
                            const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapUtilHandlerContext *handlerContext = (struct CoapUtilHandlerContext *)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader))
    {
    case OT_COAP_CODE_GET: /* A GET request */
        identityReplyHandler(handlerContext, aHeader, aMessageInfo);
        break;
    default:
        break;
    }
}
