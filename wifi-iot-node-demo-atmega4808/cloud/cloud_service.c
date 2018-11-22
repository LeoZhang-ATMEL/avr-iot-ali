/*
    \file   cloud_service.c

    \brief  Cloud Service Abstraction Layer

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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <atomic.h>
#include <avr/wdt.h>
#include "config/clock_config.h"
#include <util/delay.h>
#include <atmel_start.h>
#include "cloud_service.h"
#include "cloud/bsd_adapter/bsdWINC.h"
#include "Config/IoT_Sensor_Node_config.h"
#include "cloud/crypto_client/crypto_client.h"
#include "cloud/crypto_client/cryptoauthlib_main.h"
#include "debug_print.h"
#include "include/timeout.h"
#include "cloud/mqtt_packetPopulation/mqtt_packetPopulate.h"
#include "mqtt/mqtt_core/mqtt_core.h"
#include "wifi_service.h"

#include "application_manager.h"
#include "credentials_storage/credentials_storage.h"
 
#define CLIENT_ID_LENGTH 64 + 1 //the client id is not more than 64 bytes in aliyun document  +  '\0' (1)byte = 65bytes
#define HASH_MSG_LENGTH 72 + 40 + 21 + 22 + 1 //clientId (8+64=72)bytes  +  deviceName (10+30=40)bytes  +  productKey (10+11=21)bytes  +  timestamp (9+13=22)bytes  +  '\0' (1)byte = 156bytes

static bool cloudInitialized = false;
static bool waitingForMQTT = false;
   
//const char projectId[] = CFG_PROJECT_ID;
//const char projectRegion[] = CFG_PROJECT_REGION;
//const char registryId[] = CFG_REGISTRY_ID;
//char deviceId[CLOUD_MAX_DEVICEID_LENGTH];
const char productKey[] = CFG_PRODUCT_KEY;
const char deviceName[] = CFG_DEVICE_NAME;
const char publishTopic[] = "/sys/a11v7xKKAWn/IIIIIAVRIOTA5F0/thing/event/property/post";
const char subscribeTopic[] = "/sys/a11v7xKKAWn/IIIIIIAVRIOTA5F0/thing/service/property/set";
static char clientId[CLIENT_ID_LENGTH];
static uint8_t hashMsg[HASH_MSG_LENGTH];


// Scheduler Callback functions
absolutetime_t CLOUD_task(void *param);
absolutetime_t mqttTimeoutTask(void *payload);
absolutetime_t cloudResetTask(void *payload);

static void dnsHandler(uint8* domainName, uint32 serverIP);
static void updateJWT(uint32_t epoch);

static int8_t connectMQTTSocket(void);
static void connectMQTT();
static uint8_t reInit(void);

bool isSubscribed = false;
bool isResetting = false;
bool cloudResetTimerFlag = false;
#define CLOUD_TASK_INTERVAL            500L
#define CLOUD_MQTT_TIMEOUT_COUNT	      10000L // 10 seconds max allowed to establish a connection
#define CLOUD_RESET_TIMEOUT            2000L  // 2 seconds

#define MQTT_CONN_AGE_TIMEOUT 3600L    // 3600 seconds = 60 minutes

// Create the timers for scheduler_timeout which runs these tasks
timer_struct_t CLOUD_taskTimer            = {CLOUD_task};
timer_struct_t mqttTimeoutTaskTimer       = {mqttTimeoutTask};

timer_struct_t cloudResetTaskTimer       = {cloudResetTask};


uint32_t mqttAliIoTIP;

packetReceptionHandler_t cloud_packetReceiveCallBackTable[CLOUD_PACKET_RECV_TABLE_SIZE];

static uint8_t MQTTReceiveBuffer[48];

void CLOUD_reset(void)
{
   debug_printError("CLOUD: Cloud Reset");
	cloudInitialized = false;
}

absolutetime_t mqttTimeoutTask(void *payload) {
   debug_printError("CLOUD: MQTT Connection Timeout");
   CLOUD_reset();

   waitingForMQTT = false;
    
   return 0;  
}


absolutetime_t cloudResetTask(void *payload) {
   cloudInitialized = reInit();
   cloudResetTimerFlag = false;  
   return 0;
}


void CLOUD_init(char*  attDeviceID)
{     
   // Create timers for the application scheduler
   scheduler_timeout_create(&CLOUD_taskTimer, 500);
}

static void connectMQTT()
{
   mqttConnectPacket cloudConnectPacket;
   memset(&cloudConnectPacket, 0, sizeof(mqttConnectPacket));

   uint32_t currentTime = time(NULL);
   //if (currentTime > 0)
   //{
      // The JWT takes time in UNIX format (seconds since 1970), AVR-LIBC uses seconds from 2000 ...
      updateJWT(currentTime + UNIX_OFFSET);
   //}


   cloudConnectPacket.connectVariableHeader.connectFlagsByte.All = 0x43;//0x02
   cloudConnectPacket.connectVariableHeader.keepAliveTimer = 60;//10
   cloudConnectPacket.clientID = (uint8_t*)mqttCid;
   cloudConnectPacket.password = (uint8_t*)mqttPassword;
   cloudConnectPacket.passwordLength = strlen(mqttPassword);
   cloudConnectPacket.username = (uint8_t*)mqttUsername;
   cloudConnectPacket.usernameLength = strlen(mqttUsername);
   
   debug_printInfo("CLOUD: MQTT username=%s, password=%s",cloudConnectPacket.username,cloudConnectPacket.password);
   debug_print("CLOUD: MQTT Connect");
   MQTT_CreateConnectPacket(&cloudConnectPacket);
}

// This forces a disconnect, which forces a reconnect...
void CLOUD_disconnect(void){
    debug_printError("CLOUD: Disconnect");
    if (MQTT_GetConnectionState() == CONNECTED)
    {
        MQTT_Disconnect(MQTT_GetClientConnectionInfo());
    }
}

// Todo: This declaration supports the hack below
packetReceptionHandler_t* getSocketInfo(uint8_t sock);
static int8_t connectMQTTSocket(void)
{
   int8_t ret = false;
   
   if (mqttAliIoTIP)
   {
      struct bsd_sockaddr_in addr;
       
      addr.sin_family = PF_INET;
	  //addr.sin_port = BSD_htons(443);
      addr.sin_port = BSD_htons(1883);
      addr.sin_addr.s_addr = mqttAliIoTIP;
       
      mqttContext  *context = MQTT_GetClientConnectionInfo();
      socketState_t  socketState = BSD_GetSocketState(*context->tcpClientSocket);
       
      // Todo: Check - Are we supposed to call close on the socket here to ensure we do not leak ?
      if (socketState == NOT_A_SOCKET)
      {
         *context->tcpClientSocket = BSD_socket(PF_INET, BSD_SOCK_STREAM, 0);
         
         if (*context->tcpClientSocket >=0)
         {
            packetReceptionHandler_t*  sockInfo = getSocketInfo(*context->tcpClientSocket);
            if (sockInfo != NULL)
            {
               sockInfo->socketState = SOCKET_CLOSED;
            }
         }
      }
   
      socketState = BSD_GetSocketState(*context->tcpClientSocket);
      if (socketState == SOCKET_CLOSED) {
         debug_print("CLOUD: Connect socket");
         ret = BSD_connect(*context->tcpClientSocket, (struct bsd_sockaddr *)&addr, sizeof(struct bsd_sockaddr_in));

         if (ret != BSD_SUCCESS) {
            debug_printError("CLOUD connect received %d",ret);
            shared_networking_params.haveERROR = 1;
            BSD_close(*context->tcpClientSocket);
         }
      }
   }   
   return ret;
}

absolutetime_t CLOUD_task(void *param)
{
	 debug_printInfo("CLOUD TASK:Enter++++++++,cloudInitialized(%d),isResetting(%d),cloudResetTimerFlag(%d),waitingForMQTT(%d)", cloudInitialized , isResetting, cloudResetTimerFlag, waitingForMQTT);
	
	mqttContext* mqttConnnectionInfo = MQTT_GetClientConnectionInfo();
	volatile socketState_t socketState;

	if (!cloudInitialized)
	{
      if (!isResetting)
      { 
         isResetting = true;
		 debug_printError("CLOUD: Cloud reset timer is set");
		 scheduler_timeout_delete(&mqttTimeoutTaskTimer);
         scheduler_timeout_create(&cloudResetTaskTimer, CLOUD_RESET_TIMEOUT);
		 cloudResetTimerFlag = true;
      }      
	} else {
      if (!waitingForMQTT)
      {
         if((MQTT_GetConnectionState() != CONNECTED) && (cloudResetTimerFlag == false))
         {
            // Start the MQTT connection timeout
			debug_printError("MQTT: MQTT reset timer is created");
            scheduler_timeout_create(&mqttTimeoutTaskTimer, CLOUD_MQTT_TIMEOUT_COUNT);
            waitingForMQTT = true;
         }
      }
   }
   
   // If we have lost the AP we need to disconnect MQTT. TCP will still buffer but we know it will fail if no AP
   if (!shared_networking_params.haveAPConnection)
   {
      // This initiates a disconnection packet being sent, so it does not yet leave us disconnected!
      if (MQTT_GetConnectionState() == CONNECTED)
      {
         MQTT_Disconnect(mqttConnnectionInfo);
         shared_networking_params.haveERROR = 1;
      }
   }   
   else
   {
      static int32_t lastAge = -1;
      socketState = BSD_GetSocketState(*mqttConnnectionInfo->tcpClientSocket);

      int32_t thisAge = MQTT_getConnectionAge();
      time_t theTime = time(NULL);
      if(theTime<=0)
      {
         //debug_printError("CLOUD: time not ready");
      }
      else
      {
         if(MQTT_GetConnectionState() == CONNECTED)
         {
            if(lastAge != thisAge)
            {
               debug_printInfo("CLOUD: Uptime %lus SocketState (%d) MQTT (%d)", thisAge , socketState, MQTT_GetConnectionState());
               lastAge = thisAge;
            }
         }
      }
	  
      //debug_printInfo("CLOUD222: Uptime %lus SocketState (%d) MQTT (%d)", thisAge , socketState, MQTT_GetConnectionState());
      switch(socketState)
	   {
         case NOT_A_SOCKET:
		   case SOCKET_CLOSED:
			  // Reinitialize MQTT
			  MQTT_ClientInitialise();			   
		     connectMQTTSocket(); 
		   break;
      
		   case SOCKET_CONNECTED:
            // If MQTT was disconnected but the socket is up we retry the MQTT connection
            if (MQTT_GetConnectionState() == DISCONNECTED)
            {
               connectMQTT();
            } else {
               MQTT_ReceptionHandler(mqttConnnectionInfo);
               MQTT_TransmissionHandler(mqttConnnectionInfo);

               // Todo: We already processed the data in place using PEEK, this just flushes the buffer
               BSD_recv(*MQTT_GetClientConnectionInfo()->tcpClientSocket, MQTTReceiveBuffer, sizeof(MQTTReceiveBuffer), 0);
              
               if (MQTT_GetConnectionState() == CONNECTED)
               {
                  shared_networking_params.haveERROR = 0;
				  scheduler_timeout_delete(&mqttTimeoutTaskTimer);
				  scheduler_timeout_delete(&cloudResetTaskTimer);
				  isResetting = false;
                  waitingForMQTT = false;
                   
                  // The Authorization timeout is set to 3600, so we need to re-connect that often
                  if (MQTT_getConnectionAge() > MQTT_CONN_AGE_TIMEOUT) {
                     MQTT_Disconnect(mqttConnnectionInfo);
                     BSD_close(*mqttConnnectionInfo->tcpClientSocket);
                  }
               } 
            }
		   break;

		   default:
            shared_networking_params.haveERROR = 1; 
		   break;
	   }
   }   
   
    debug_printInfo("CLOUD TASK:Exit --------,cloudInitialized(%d),isResetting(%d),cloudResetTimerFlag(%d),waitingForMQTT(%d)", cloudInitialized , isResetting, cloudResetTimerFlag, waitingForMQTT);
	return CLOUD_TASK_INTERVAL;
}

bool CLOUD_isConnected(void)
{
   if (MQTT_GetConnectionState() == CONNECTED)
   {
      return true;
   } else {
      return false;
   }
}

void CLOUD_publishData(uint8_t* data, unsigned int len)
{
   MQTT_CLIENT_publish(data, len);
}

void CLOUD_subscribeData(uint8_t* data, unsigned int len)
{
   MQTT_CLIENT_subscribe(data, len);
}

static void dnsHandler(uint8* domainName, uint32 serverIP)
{
    if(serverIP != 0)
    {
        mqttAliIoTIP = serverIP;
        debug_printInfo("CLOUD: mqttAliIoTIP = (%lu.%lu.%lu.%lu)",(0x0FF & (serverIP)),(0x0FF & (serverIP>>8)),(0x0FF & (serverIP>>16)),(0x0FF & (serverIP>>24)));
    }
}

//static void updateJWT(uint32_t epoch)
//{
   //char ateccsn[20];
   //CRYPTO_CLIENT_printSerialNumber(ateccsn);
   //sprintf(deviceId, "d%s", ateccsn);
//
   //sprintf(mqttCid, "projects/%s/locations/%s/registries/%s/devices/%s", projectId, projectRegion, registryId, deviceId);
   //sprintf(mqttTopic, "/devices/%s/events", deviceId);
   //
   //debug_printInfo("MQTT: mqttCid=%s", mqttCid);
   //debug_printInfo("MQTT: mqttTopic=%s", mqttTopic);
   //uint8_t res = CRYPTO_CLIENT_createJWT((char*)mqttPassword, PASSWORD_SPACE, epoch, projectId);
   //time_t t = epoch - UNIX_OFFSET;
   //debug_printInfo("JWT: Result(%d) at %s", res==0? 1 : -1, ctime(&t));
//}

static void updateJWT(uint32_t epoch)
{
	//char ateccsn[20];
	//CRYPTO_CLIENT_printSerialNumber(ateccsn);
	//sprintf(deviceId, "d%s", ateccsn);

	sprintf(clientId, "%s&%s_aliyun-iot-device-sdk-js", productKey, deviceName);
	sprintf(mqttCid, "%s|securemode=3,signmethod=hmacsha256,timestamp=1539939251799|", clientId);
	sprintf(mqttPublishTopic, "/sys/%s/%s/thing/event/property/post", productKey, deviceName);
	sprintf(mqttSubscribeTopic, "/sys/%s/%s/thing/service/property/set", productKey, deviceName);
	sprintf(mqttUsername, "%s&%s", deviceName, productKey);
	sprintf(hashMsg, "clientId%sdeviceName%sproductKey%stimestamp1539939251799", clientId, deviceName, productKey);
	size_t hashMsgLen = strlen(hashMsg);
	CRYPTO_CLIENT_printHMACSHA256(hashMsg, hashMsgLen, mqttPassword);
		
	debug_printInfo("MQTT: hashMsg=%s, hashMsglength=%d", hashMsg, hashMsgLen);	
	debug_printInfo("MQTT: mqttCid=%s, mqttCidlength=%d", mqttCid,strlen(mqttCid));
	debug_printInfo("MQTT: mqttPublishTopic=%s", mqttPublishTopic);
	debug_printInfo("MQTT: mqttSubscribeTopic=%s", mqttSubscribeTopic);
	//uint8_t res = CRYPTO_CLIENT_createJWT((char*)mqttPassword, PASSWORD_SPACE, epoch, projectId);
	//time_t t = epoch - UNIX_OFFSET;
	//debug_printInfo("JWT: Result(%d) at %s", res==0? 1 : -1, ctime(&t));
}

static uint8_t reInit(void)
{
    debug_printInfo("CLOUD: reinit");
    
    mqttAliIoTIP = 0;
    shared_networking_params.haveAPConnection = 0;
    waitingForMQTT = false;
    isResetting = false;
	isSubscribed = false;
    
	 // Re-init the WiFi
	 wifi_reinit();
	 
    registerSocketCallback(BSD_SocketHandler, dnsHandler);

    MQTT_ClientInitialise();
    memset(&cloud_packetReceiveCallBackTable, 0, sizeof(cloud_packetReceiveCallBackTable));
    BSD_SetRecvHandlerTable(cloud_packetReceiveCallBackTable);
    
    cloud_packetReceiveCallBackTable[0].socket = MQTT_GetClientConnectionInfo()->tcpClientSocket;
    cloud_packetReceiveCallBackTable[0].recvCallBack = MQTT_CLIENT_receive;

    int8_t e;
    debug_print("CLOUD: credentials %s, %s,%s", ssid,pass,authType);
    if(M2M_SUCCESS != (e=m2m_wifi_connect((char *)ssid, sizeof(ssid), atoi((char*)authType), (char *)pass, M2M_WIFI_CH_ALL)))
    {
        debug_printError("CLOUD: wifi error = %d",e);
        shared_networking_params.haveERROR = 1;
        return false;
    }

    return true;
}
