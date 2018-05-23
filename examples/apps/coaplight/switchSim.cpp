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

#include <openthread-core-config.h>
#include <openthread/config.h>

#include <openthread/cli.h>
#include <openthread/coap.h>
#include <openthread/diag.h>
#include <openthread/openthread.h>
#include <openthread/platform/logging.h>
// The following define was added to fix compilation error when including the common/instance.hpp
#define OPENTHREAD_FTD 1
#include "common/encoding.hpp"
#include "common/instance.hpp"
#include "common/timer.hpp"

using ot::Encoding::BigEndian::HostSwap16;
using ot::Instance;
using ot::Timer;
using ot::TimerMilli;

#define SWITCHSIM_DEFAULT_FREQ 5000
#define SWITCHSIM_START_DELAY 0
#define SWITCHSIM_MAX_FREQ 255000

void    setUpTimer(otInstance *aInstance, bool firstTime, TimerMilli *timer);
void    TimerFired(Timer &aTimer);
otError ParseUInt32(char *argv, uint32_t &value);

struct SwitchSimContext
{
    otInstance *  aInstance;
    TimerMilli *  gTimer;
    otIp6Address *gLightIp;
    uint32_t      gSwitchFrequency;
};

/**
 * The instance of our context information structure.
 */
static struct SwitchSimContext switchSimContext = {.aInstance        = NULL,
                                                   .gTimer           = NULL,
                                                   .gLightIp         = NULL,
                                                   .gSwitchFrequency = 0};

void ProcessSwitchSim(int argc, char *argv[])
{
    otError      error = OT_ERROR_NONE;
    otIp6Address lightIp;
    bool         isStart, isStop, isHelp;
    isStart = isStop = isHelp = false;

    VerifyOrExit(argc > 0, error = OT_ERROR_INVALID_ARGS);
    isStart = (strcmp(argv[0], "start") == 0);
    isStop  = (strcmp(argv[0], "stop") == 0);
    isHelp  = (strcmp(argv[0], "help") == 0);
    VerifyOrExit(isStart || isStop || isHelp, error = OT_ERROR_INVALID_ARGS);

    if (isStart)
    {
        VerifyOrExit(argc > 1, error = OT_ERROR_INVALID_ARGS);
        SuccessOrExit(error = otIp6AddressFromString(argv[1], &lightIp));
        if (switchSimContext.gLightIp == NULL)
            switchSimContext.gLightIp = new otIp6Address();
        memcpy(switchSimContext.gLightIp->mFields.m8, lightIp.mFields.m8,
               sizeof(switchSimContext.gLightIp->mFields.m8));

        switchSimContext.gSwitchFrequency = SWITCHSIM_DEFAULT_FREQ;
        if (argc > 2)
        {
            uint32_t freqValue = 0;
            SuccessOrExit(error = ParseUInt32(argv[2], freqValue));
            VerifyOrExit(freqValue > 0 && freqValue < SWITCHSIM_MAX_FREQ, error = OT_ERROR_INVALID_ARGS);
            switchSimContext.gSwitchFrequency = freqValue;
        }

        char ippddrString[64];
        ippddrString[0] = '\0';
        snprintf(ippddrString, sizeof(ippddrString) - 1, "%x:%x:%x:%x:%x:%x:%x:%x",
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[0]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[1]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[2]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[3]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[4]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[5]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[6]),
                 HostSwap16(switchSimContext.gLightIp->mFields.m16[7]));
        otCliUartOutputFormat("SUCCESS, starting switchsim app on ip = %s", ippddrString);
        setUpTimer(switchSimContext.aInstance, true, switchSimContext.gTimer);
    }
    if (isStop)
    {
        switchSimContext.gTimer->Stop();
        otCliUartOutputFormat("SUCCESS, stopping switchsim app");
    }
    if (isHelp)
    {
        otCliUartOutputFormat("Use: switchsim {stop|start} <ipv6addr>, eg. 'switchsim start ::1' or 'switchsim stop'");
    }

exit:
    if (error != OT_ERROR_NONE)
        otCliUartOutputFormat("ERROR, in switchsim command: %s", otThreadErrorToString(error));
    return;
}

const otCliCommand gCustomCommands[] = {{"switchsim", &ProcessSwitchSim}};

void setUpTimer(otInstance *aInstance, bool firstTime, TimerMilli *timer)
{
    (void)firstTime;
    (void)aInstance;
    timer->Stop();
    if (firstTime)
    {
        timer->StartAt(TimerMilli::GetNow() + SWITCHSIM_START_DELAY, switchSimContext.gSwitchFrequency);
        firstTime = false;
    }
    else
    {
        timer->Start(switchSimContext.gSwitchFrequency);
    }
}

void switchSimInit(otInstance *aInstance)
{
    Instance &sInstance        = *static_cast<Instance *>(aInstance);
    switchSimContext.aInstance = aInstance;
    switchSimContext.gTimer    = new TimerMilli(sInstance, TimerFired, NULL);
    otCliUartSetUserCommands(gCustomCommands, 1);
}

void TimerFired(Timer &aTimer)
{
    (void)aTimer;
    if (switchSimContext.gLightIp == NULL)
        return; // This will stop the periodic timer
    otCoapHeader  header;
    otMessageInfo messageInfo;
    otMessage *   message = NULL;
    otError       error   = OT_ERROR_NONE;

    otCoapHeaderInit(&header, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);
    otCoapHeaderGenerateToken(&header, ot::Coap::Header::kDefaultTokenLength);
    SuccessOrExit(error = otCoapHeaderAppendUriPathOptions(&header, "light/toggle"));
    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.mPeerAddr    = *(switchSimContext.gLightIp);
    messageInfo.mPeerPort    = OT_DEFAULT_COAP_PORT;
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
    message                  = otCoapNewMessage(switchSimContext.aInstance, &header);
    VerifyOrExit(message != NULL, error = OT_ERROR_NO_BUFS);
    error = otCoapSendRequest(switchSimContext.aInstance, message, &messageInfo, NULL, NULL);
    otCliUartOutputFormat("SwitchSim: Sending PUT light/toggle msg");

exit:
    if ((error != OT_ERROR_NONE) && (message != NULL))
    {
        otMessageFree(message);
    }
    setUpTimer(switchSimContext.aInstance, false, switchSimContext.gTimer);
}

void switchSimCleanup()
{
    if (switchSimContext.gTimer != NULL)
        delete switchSimContext.gTimer;
}

otError ParseUInt32(char *argv, uint32_t &value)
{
    char *endptr;
    long  temp;
    temp  = strtol(argv, &endptr, 0);
    value = (uint32_t)(temp < 0 ? 0 : temp);
    return (*endptr == '\0') ? OT_ERROR_NONE : OT_ERROR_PARSE;
}
