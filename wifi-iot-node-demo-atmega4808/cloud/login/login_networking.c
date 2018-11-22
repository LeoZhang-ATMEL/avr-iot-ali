/*
    \file   login_networking.c

    \brief  Login networking source file.

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
#include "../login/login.h"
#include "../ntp/ntp.h"
#include "login_networking.h"
#include "../cloud/cloud.h"
#include "../bsd_adapter/bsdWINC.h"

#define MAIN_WIFI_M2M_BUFFER_SIZE		48
uint8_t socketBuffer[MAIN_WIFI_M2M_BUFFER_SIZE];


void LOGIN_NETWORKING_socketHandler(SOCKET sock, uint8_t msgType, void *pMsg)
{
    int16_t ret;
    uint8_t *packetBuffer;
    tstrSocketBindMsg *pstrBind;
    tstrSocketRecvMsg *pstrRx;
	struct bsd_sockaddr_in addr;
    
    addr.sin_family = PF_INET;
    
    switch (msgType) {
        case SOCKET_MSG_BIND:
            pstrBind = (tstrSocketBindMsg *)pMsg;
            if (pstrBind && pstrBind->status == 0) {
				socklen_t sendSize = sizeof(struct bsd_sockaddr_in);
				packetBuffer = NTP_getSocketReceiveBuffer();
				ret = BSD_recvfrom(sock, (void *)packetBuffer, MAIN_WIFI_M2M_BUFFER_SIZE, 0, (struct bsd_sockaddr *)&addr, &sendSize);
                if (ret != SOCK_ERR_NO_ERROR) 
                {
                    LOGIN_receiveEvent(LOGIN_SOCKET_BIND_FAILED_E);
                } 
                else 
                {
                    LOGIN_receiveEvent(LOGIN_SOCKET_BOUND_E);
                }
            } 
            else 
            {
                LOGIN_receiveEvent(LOGIN_SOCKET_BIND_FAILED_E);
            }
            break;
        
        case SOCKET_MSG_RECVFROM:
            pstrRx = (tstrSocketRecvMsg *)pMsg;
            if (pstrRx->pu8Buffer && pstrRx->s16BufferSize) 
            {
				if ((pstrRx->pu8Buffer[0] & 0x7) != 4)
                {
                    LOGIN_receiveEvent(LOGIN_NTP_REQ_FAILED_E);
                } 
                else 
                {
					const packetReceptionHandler_t *core_bsdRecvInfo;
					core_bsdRecvInfo = BSD_GetRecvHandlerTable();
					for(uint8_t i = 0; i < CLOUD_PACKET_RECV_TABLE_SIZE; i++)
					{
						if(core_bsdRecvInfo)
						{
							if(*(core_bsdRecvInfo->socket) == sock)
							{
								packetBuffer = NTP_getSocketReceiveBuffer();
								memcpy(packetBuffer, pstrRx->pu8Buffer, MAIN_WIFI_M2M_BUFFER_SIZE);
								core_bsdRecvInfo->recvCallBack(packetBuffer, MAIN_WIFI_M2M_BUFFER_SIZE);
								break;
							}
						}
						core_bsdRecvInfo++;
					}
                }
            }
            break;
             
        default:
            break;
    }
}
