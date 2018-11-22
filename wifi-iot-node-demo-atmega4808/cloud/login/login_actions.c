/*
    \file   login_actions.c

    \brief  Login actions source file.

    (c) 2018 Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip software and any
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party
    license terms applicable to your use of third party software (including open source software) that
    may accompany Microchip software.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
    FOR A PARTICULAR PURPOSE.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
    SOFTWARE.
*/

#include <stdint.h>
#include <string.h>
#include "../../winc/socket/include/socket.h"
#include "../ntp/ntp.h"
#include "../mqtt_packetPopulation/mqtt_packetPopulate.h"
#include "../crypto_client/crypto_client.h"
#include "login.h"
#include "timeout.h"
#include "login_actions.h"
#include "../bsd_adapter/bsdWINC.h"

#define DEFAULT_WAIT_TIME 5000
#define PASSWORD_SPACE 456

static absolutetime_t wait_timer_cb(void *payload);
static timer_struct_t wait_timer = {wait_timer_cb};
static void startWaiting();


loginState_t LOGIN_ACTIONS_generateJWT(void)
{
    if(CRYPTO_CLIENT_createJWT((char*)mqttPassword, PASSWORD_SPACE, (uint32_t)NTP_getEpoch()) != NO_ERROR)
    {
        return LOGIN_GENERATION_FAILED_S;
    }
    else
    {
        return LOGIN_GENERATED_S;
    }
}

static absolutetime_t wait_timer_cb(void *payload)
{
    LOGIN_receiveEvent(LOGIN_TIMEOUT_E);
    return 0;
}

static void startWaiting()
{
    scheduler_timeout_delete(&wait_timer);
    scheduler_timeout_create(&wait_timer, DEFAULT_WAIT_TIME);
}
