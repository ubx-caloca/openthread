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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <openthread/types.h>
#include <openthread/coap.h>
#include <openthread/openthread.h>
#include <openthread/platform/logging.h>

#include <openthread/link.h>
#include <openthread/cli.h>
#include <openthread/ip6.h>
#include "jsmn.h"

#define LIGHT_DEFAULT_LEVEL 0
#define LIGHT_MAX_LEVEL 255
#define LIGHT_DEFAULT_TOGGLELEVEL LIGHT_MAX_LEVEL
#define LIGHT_DEFAULT_STEP LIGHT_MAX_LEVEL

#define APP_REQPAYLOAD_SIZE 512
#define APP_RESPPAYLOAD_SIZE 200
#define APP_MAXSTRING_ATTR 30
#define APP_JSONPARSING_NUMTOKENS 10
#define APP_MAXSTRING_ATTRNAMEVAL 50

//prototypes
static void getSubString(char *bigString, uint16_t startIndex, uint16_t size, char *subString);
static void coapPayloadMakeStringSafe(char *payload, uint16_t payloadLength);

class Light {
public:
    uint8_t level;
    uint8_t toggleLevel;
    uint8_t step;
    Light(){
        level = LIGHT_DEFAULT_LEVEL;
        toggleLevel = LIGHT_DEFAULT_TOGGLELEVEL;
        step = LIGHT_DEFAULT_STEP;
    }
};
/**
 * Context information to be passed to our handlers.
 */
struct CoapLightHandlerContext {
    otInstance *aInstance;
    Light *lights[1];
};

/**
 * The instance of our context information structure.
 */
static struct CoapLightHandlerContext lightContext =
{
    .aInstance = NULL,
    .lights = {NULL}
};

/**
 * Handler for the light path.
 */
static void lightHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the light path.
 */
static otCoapResource lightResource = {
    .mUriPath = "light",
    .mHandler = &lightHandler,
    .mContext = (void*)&lightContext,
    .mNext = NULL
};

/**
 * Handler for the light/toogle path.
 */
static void lightToogleHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the light/toogle path.
 */
static otCoapResource lightToogleResource = {
    .mUriPath = "light/toggle",
    .mHandler = &lightToogleHandler,
    .mContext = (void*)&lightContext,
    .mNext = NULL
};

/**
 * Handler for the light/on path.
 */
static void lightUpHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the light/on path.
 */
static otCoapResource lightUpResource = {
    .mUriPath = "light/up",
    .mHandler = &lightUpHandler,
    .mContext = (void*)&lightContext,
    .mNext = NULL
};

/**
 * Handler for the light/off path.
 */
static void lightDownHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the light/off path.
 */
static otCoapResource lightDownResource = {
    .mUriPath = "light/down",
    .mHandler = &lightDownHandler,
    .mContext = (void*)&lightContext,
    .mNext = NULL
};

/**
 * Handler for the light/set path.
 */
static void lightSetHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/**
 * Resource definition for the Light/on path.
 */
static otCoapResource lightSetResource = {
    .mUriPath = "light/set",
    .mHandler = &lightSetHandler,
    .mContext = (void*)&lightContext,
    .mNext = NULL
};

void coapLightInit(otInstance *aInstance)
{
    lightContext.aInstance = aInstance;
    lightContext.lights[0] = new Light();
    otCoapAddResource(aInstance, &lightResource);
    otCoapAddResource(aInstance, &lightToogleResource);
    otCoapAddResource(aInstance, &lightUpResource);
    otCoapAddResource(aInstance, &lightDownResource);
    otCoapAddResource(aInstance, &lightSetResource);
//    return otCoapStart(aInstance, OT_DEFAULT_COAP_PORT);
}


/**
 * Defining how to reply to a PUT light/toogle, light/on and light/off requests… we 
 * do this here as we'll need to do it a few times.
 */
static void lightReplyHandler(struct CoapLightHandlerContext *handlerContext,
        otCoapHeader *aHeader, otMessage *aMessage, const otMessageInfo *aMessageInfo, const bool isGet, const char *path)
{
    otCoapHeader replyHeader;
    otMessage *replyMessage;

    /*
     * Reply is an ACK with content to come
     */
    otCoapHeaderInit(
            &replyHeader,
            OT_COAP_TYPE_ACKNOWLEDGMENT,
            OT_COAP_CODE_CONTENT
    );

    /*
     * Copy the token from the request header
     */
    otCoapHeaderSetToken(&replyHeader,
            otCoapHeaderGetToken(aHeader),
            otCoapHeaderGetTokenLength(aHeader)
    );

    /*
     * Copy the message ID from the request header
     */
    otCoapHeaderSetMessageId(
            &replyHeader,
            otCoapHeaderGetMessageId(aHeader)
    );

    /*
     * Set the Content Format option to JSON
     */
    otCoapHeaderAppendContentFormatOption(&replyHeader, OT_COAP_OPTION_CONTENT_FORMAT_JSON);

    /*
     * We're generating a piggy-back response, set the marker indicating this
     * or we'll get a PARSE error when we try to send.
     */
    otCoapHeaderSetPayloadMarker(&replyHeader);

    replyMessage = otCoapNewMessage(
            handlerContext->aInstance, &replyHeader
    );

    if (replyMessage)
    {
        otError result;
	char response[APP_RESPPAYLOAD_SIZE];
        int16_t newToggleLevel  = -1;
        int16_t newStep  = -1;
        bool toggleLevelChanged  = false;
        bool stepChanged  = false;		
	uint8_t status = 0; //0 -> Error, 1 -> Ok

        if(!isGet){
            //If PUT, change light state first
            if(strcmp(path, "light/toggle") == 0){
                if(handlerContext->lights[0]->level == 0)
		    handlerContext->lights[0]->level = handlerContext->lights[0]->toggleLevel;
		else
                    handlerContext->lights[0]->level = 0;
                status = 1; //Ok
            }
            if(strcmp(path, "light/up") == 0){
		int16_t newLevel = handlerContext->lights[0]->level + handlerContext->lights[0]->step;
                handlerContext->lights[0]->level = newLevel > LIGHT_MAX_LEVEL ? LIGHT_MAX_LEVEL : (uint8_t)newLevel;
                status = 1; //Ok
            }
            if(strcmp(path, "light/down") == 0){
	        int16_t newLevel = handlerContext->lights[0]->level - handlerContext->lights[0]->step;
                handlerContext->lights[0]->level = newLevel < 0 ? 0 : (uint8_t)newLevel;
                status = 1; //Ok
            }
            if(strcmp(path, "light/set") == 0){ 
                //TODO: Check COAP option if payload is JSON
		//Because of the coap content format appending issue of the request,
		//We will not check the contentformat to be json and assume that the 
		//payload is a json
/*		
		const otCoapOption *curOption = otCoapHeaderGetFirstOption(aHeader);
		uint8_t numOptions = 0;
		while(curOption != NULL){
		    //if(curOption->mLength == 8)
		    	otCliUartOutputFormat("| option{mNumber=%d, mLength=%d, Val=%02X}", curOption->mNumber, curOption->mLength, curOption->mValue[0]);
		    curOption = otCoapHeaderGetNextOption(aHeader);
		    numOptions++;
		}
otCliUartOutputFormat("Num options = %d", numOptions); 
*/
 		uint16_t payloadLength = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);
    		char buf[APP_REQPAYLOAD_SIZE];

		if(payloadLength <= sizeof(buf)  && payloadLength > 0){
	 	    otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, payloadLength);
                    coapPayloadMakeStringSafe(buf, payloadLength);

                    //Parsing json
		    jsmn_parser parser;
		    jsmntok_t tokens[APP_JSONPARSING_NUMTOKENS];
		    jsmn_init(&parser);
		    int16_t numTokens = jsmn_parse(&parser, buf, strlen(buf), tokens, APP_JSONPARSING_NUMTOKENS);

		    if(numTokens > 0){
		        if(tokens[0].type == JSMN_OBJECT){
                            bool checkNextTokens = true;
			    //Checking the other tokens between start and end of token 0
		    	    for(uint8_t i=1; i<numTokens; i++){
                                if(!(tokens[i].start > tokens[0].start && tokens[i].end < tokens[0].end)){
                                    checkNextTokens = false;
		                    break;
                                }    
			    }
                            if(checkNextTokens){
                                //Checking the name values pairs of token to get values
                                bool tokenVal = false;
                                int16_t attrNameStart = -1;
                                int16_t attrNameEnd = -1;
		    	        for(uint8_t i=1; i<numTokens; i++){
                                    if(!tokenVal){
					//Processing Attr Name
		                        if(tokens[i].type != JSMN_STRING){
                                            break;
                                        }
                                        else{
                                            attrNameStart = tokens[i].start;
                                            attrNameEnd = tokens[i].end;
                                        }
                                        tokenVal = true;
                                    }
                                    else{
					//Processing Attr Value
                                        if(tokens[i].type != JSMN_PRIMITIVE){
                                            break;
                                        }
                                        if(!(buf[tokens[i].start] == '-' || (buf[tokens[i].start] >= '0' && buf[tokens[i].start] <= '9'))){
//If not a number
                                            break;
                                        }
					if(attrNameStart<0 || attrNameEnd<0)
					    break;
                                        char attrValString[APP_MAXSTRING_ATTR];
                                        getSubString(buf, tokens[i].start, tokens[i].end - tokens[i].start, attrValString);
                                        char attrNameString[APP_MAXSTRING_ATTR];
                                        getSubString(buf, attrNameStart, attrNameEnd - attrNameStart, attrNameString);

                                        //From here on, error when processing attr value is only a fail for that attr in the json, continue with the other attrs 
                                        tokenVal = false; 
                                        attrNameStart = attrNameEnd = -1;

					char *end = attrValString;

                                        int32_t val = (int32_t)strtol(attrValString, &end, 10);
                                        if(*end != '\0')
                                            continue; //Parse failed
                                        
                                        if(strcmp(attrNameString, "step") == 0){
                                            if(val <= 0 || val > LIGHT_MAX_LEVEL)
                                                continue;
                                            newStep = val;
                                        }
                                        if(strcmp(attrNameString, "toggleLevel") == 0){
                                            if(val <= 0 || val > LIGHT_MAX_LEVEL)
                                                continue;
                                            newToggleLevel = val;
                                        }
                                    }
			        }//end if (tokenVal == true)
                            }//end for tokens[]
		        }//end if(tokens[0].type == JSMN_OBJECT)
		    }//end if(numTokens > 0)

		} //end if(payloadLength <= sizeof(buf)  && payloadLength>0)

                //Now Try to change values
               if(newStep < 0 && newToggleLevel < 0){
                    //Request payload is not json or json is formatted incorrectly
                    status = 0; //Errror    
                }
                else{
                    //newStep >= 0 || newToggleLevel >= 0
                    //At least one attr was processed, return status as Ok
		    status = 1;
		    //Now try to change values
                    if(newStep >= 0){
                        if(newStep != handlerContext->lights[0]->step){
                            handlerContext->lights[0]->step = newStep;
                            stepChanged = true;
                        }
                    }
                    if(newToggleLevel >= 0){
                        if(newToggleLevel != handlerContext->lights[0]->toggleLevel){
                            handlerContext->lights[0]->toggleLevel = newToggleLevel;
                            toggleLevelChanged = true;
                        }
                    }
                }
            }

        }
        int len = 0;

        //Construct response
        if(isGet){
            len = snprintf(response, sizeof(response)-1,
                "{\"level\": %d, \"toggleLevel\": %d, \"step\": %d}", 
                     handlerContext->lights[0]->level,
                     handlerContext->lights[0]->toggleLevel,
                     handlerContext->lights[0]->step);
        }
        else{//PUT
            if(strcmp(path, "light/set") == 0){
                //Constructing response for light/set
                char stepBuf[APP_MAXSTRING_ATTRNAMEVAL];
                stepBuf[0] = '\0';
                char toogleLevelBuf[APP_MAXSTRING_ATTRNAMEVAL];
                toogleLevelBuf[0] = '\0';
                if(stepChanged){
                    len = snprintf(stepBuf, sizeof(stepBuf)-1,
                    "\"step\":%d", 
                     handlerContext->lights[0]->step);
                }
                if(toggleLevelChanged){
                    len = snprintf(toogleLevelBuf, sizeof(toogleLevelBuf)-1,
                    "\"toggleLevel\":%d", 
                     handlerContext->lights[0]->toggleLevel);
                }                
                len = snprintf(response, sizeof(response)-1,
                    "{\"status\":\"%s\"%s%s%s%s}", 
                     (status == 1 ? "Ok":"Error"),
                     ((!stepChanged && !toggleLevelChanged) ? "":" , "), 
                     (stepChanged? stepBuf: ""),
                     (stepChanged && toggleLevelChanged? " , ": ""),
                     (toggleLevelChanged? toogleLevelBuf: "")
                     ); 
            }
            else{
                // light/up, light/down, light/toggle
                len = snprintf(response, sizeof(response)-1,
                    "{\"status\":\"%s\", \"level\":%d}", 
                     (status == 1 ? "Ok":"Error"), 
                     handlerContext->lights[0]->level);  
            }          
        }
	result = otMessageAppend(
                replyMessage, response, len
	);

        if (result == OT_ERROR_NONE)
        {
            /* All good, now send it */
            result = otCoapSendResponse(
                    handlerContext->aInstance, replyMessage,
                    aMessageInfo
            );
	   otCliUartOutputFormat("Got COAP message /%s, replying '%s'", path, response);
        }

        if (result != OT_ERROR_NONE)
        {
            /* There was an issue above, free up the message */
            otMessageFree(replyMessage);
        }
    }
}

static void lightHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapLightHandlerContext *handlerContext =
        (struct CoapLightHandlerContext*)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader)) {
        case OT_COAP_CODE_GET:   /* A GET request */
            lightReplyHandler(handlerContext, aHeader, aMessage, aMessageInfo, true, "light");
            break;
        default:
            break;
    }
}

static void lightToogleHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapLightHandlerContext *handlerContext =
        (struct CoapLightHandlerContext*)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader)) {
        case OT_COAP_CODE_PUT:   /* A PUT request */
            lightReplyHandler(handlerContext, aHeader, aMessage, aMessageInfo, false, "light/toggle");
            break;
        default:
            break;
    }
}

static void lightUpHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    //(void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapLightHandlerContext *handlerContext =
        (struct CoapLightHandlerContext*)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader)) {
        case OT_COAP_CODE_PUT:  /* A PUT request */
            {
                lightReplyHandler(handlerContext, aHeader, aMessage, aMessageInfo, false, "light/up");
            }
            break;
        default:
            break;
    }
}

static void lightDownHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapLightHandlerContext *handlerContext =
        (struct CoapLightHandlerContext*)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader)) {
        case OT_COAP_CODE_PUT:  /* A PUT request */
            {
                lightReplyHandler(handlerContext, aHeader, aMessage, aMessageInfo, false, "light/down");
            }
            break;
        default:
            break;
    }
}

static void lightSetHandler(
        void *aContext, otCoapHeader *aHeader, otMessage *aMessage,
        const otMessageInfo *aMessageInfo)
{
    /* We ignore the message content */
    (void)aMessage;

    /* Pick up our context passed in earlier */
    struct CoapLightHandlerContext *handlerContext =
        (struct CoapLightHandlerContext*)aContext;

    if (otCoapHeaderGetType(aHeader) != OT_COAP_TYPE_CONFIRMABLE)
    {
        /* Not a confirmable request, so ignore it. */
        return;
    }

    switch (otCoapHeaderGetCode(aHeader)) {
        case OT_COAP_CODE_PUT:  /* A PUT request */
            {
                lightReplyHandler(handlerContext, aHeader, aMessage, aMessageInfo, false, "light/set");
            }
            break;
        default:
            break;
    }
}

static void getSubString(
        char *bigString, uint16_t startIndex, uint16_t size, char *subString)
{
    memcpy(subString, &(bigString[startIndex]), size);
    subString[size] = '\0';
}

static void coapPayloadMakeStringSafe(
        char *payload, uint16_t payloadLength)
{
    if (payload[payloadLength - 1] == '\n')
    { 
        payload[--payloadLength] = '\0';
    }

    if (payload[payloadLength - 1] == '\r')
    {
        payload[--payloadLength] = '\0';
    }
    payload[payloadLength]='\0';
}


