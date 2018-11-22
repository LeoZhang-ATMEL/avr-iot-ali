#ifndef IOT_SENSOR_NODE_CONFIG_H
#define IOT_SENSOR_NODE_CONFIG_H

#include "../winc/driver/include/m2m_wifi.h"

// <h> Application Configuration

// <o> Send Interval <0-100000>
// <i> Send interval in seconds
// <id> application_send_interval
#define CFG_SEND_INTERVAL 5

// <o> Timeout <0-100000>
// <i> Timeout
// <id> application_timeout
#define CFG_TIMEOUT 5000

// </h>

// <h> WLAN Configuration

// <s> SSID
// <i> Target WLAN SSID
// <id> main_wlan_ssid
#define CFG_MAIN_WLAN_SSID "Microchip0"
//#define CFG_MAIN_WLAN_SSID "avrtoaliyun"
//#define CFG_MAIN_WLAN_SSID "360avraliyun"
// <y> Authentication
// <i> Target WLAN Authentication
// <M2M_WIFI_SEC_INVALID"> Invalid security type
// <M2M_WIFI_SEC_OPEN"> Wi-Fi network is not secured
// <M2M_WIFI_SEC_WPA_PSK"> Wi-Fi network is secured with WPA/WPA2 personal(PSK)
// <M2M_WIFI_SEC_WEP"> Security type WEP (40 or 104) OPEN OR SHARED
// <M2M_WIFI_SEC_802_1X"> Wi-Fi network is secured with WPA/WPA2 Enterprise.IEEE802.1x user-name/password authentication
// <id> main_wlan_auth
#define CFG_MAIN_WLAN_AUTH M2M_WIFI_SEC_WPA_PSK

// <s> Password
// <i> Target WLAN password
// <id> main_wlan_psk
#define CFG_MAIN_WLAN_PSK "12345678"

// </h>

// <h> Cloud Configuration

// <s> project id
// <i> Google Cloud Platform project id
// <id> project_id
//#define CFG_PROJECT_ID "avr-iot"
// Link Platform
#define CFG_PRODUCT_KEY "a1mzSZNEDrg"
// Link Develope
//#define CFG_PRODUCT_KEY "a1w1r23v8G6"

// <s> registry id
// <i> Google Cloud Platform registry id
// <id> registry_id
//#define CFG_REGISTRY_ID "AVR-IOT"
#define CFG_DEVICE_NAME "wzmSebJtDRhX17RBmKdi"

// <s> registry id
// <i> Google Cloud Platform registry id
// <id> registry_id
//#define CFG_REGISTRY_ID "AVR-IOT"
#define CFG_DEVICE_SECURE "ZMtp54TMJ8H6Ymjjp34zJbbh4Ts0NZSQ"

// <s> mqtt host
// <i> mqtt host address
// <id> mqtt_host
//#define CFG_MQTT_HOST "a1Jb91LdXkv.iot-as-mqtt.cn-shanghai.aliyuncs.com" //"mqtt.googleapis.com"
#define CFG_MQTT_HOST_SUFFIX ".iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define CFG_MQTT_HOST CFG_PRODUCT_KEY CFG_MQTT_HOST_SUFFIX

// <s> project region
// <i> Google Cloud Platform project region
// <id> project_region
//#define CFG_PROJECT_REGION "us-central1"
// </h>

// <h> AP Mode Configuration

// <s> Name
// <i> AP name
// <id> wlan_ap_name
//#define CFG_WLAN_AP_NAME "AVR.IoT"

// <s> IP Address
// <i> AP IP Address
// <id> wlan_ap_ip_address
//#define CFG_WLAN_AP_IP_ADDRESS {192, 168, 1, 1}

// </h>

// <h> Debug Configuration

// <q> Enable debug messages:
// <i> Check to enable debug messages
// <id> debug_msg
#define CFG_DEBUG_MSG 1

// </h>


#endif // IOT_SENSOR_NODE_CONFIG_H
