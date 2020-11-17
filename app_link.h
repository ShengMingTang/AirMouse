#ifndef APP_LINK_H_
#define APP_LINK_H_

#ifdef __cplusplus
extern "C"{
#endif

// device specific
#define NETWORK_PIN (14)
#define NETWORK_AP_ON_VALUE (0)
#define NETWORK_P2P_ON_VALUE (1)
#define NETWORK_VALUE_NONE (2)

// P2P defines
#define P2P_REMOTE_DEVICE   "remote-p2p-device"
#define P2P_DEVICE_NAME     "Air-Mouse"
#define P2P_SECURITY_TYPE   SL_SEC_TYPE_P2P_PBC
#define P2P_SECURITY_KEY    ""
#define P2P_DEVICE_TYPE    "2-0050F204-2"
#define P2P_ROLE            SL_P2P_ROLE_GROUP_OWNER
#define P2P_NEG_INITIATOR   SL_P2P_NEG_INITIATOR_ACTIVE
#define LISENING_CHANNEL    11
#define REGULATORY_CLASS    81
#define OPERATING_CHANNEL   6
// AP defines
#define AP_SSID "Air-Mouse"

// tasks
void linkLayerControlTask(void * pvParameters); // P2P and AP
void P2PManagerTask(void * pvParameters);
void APTask(void * pvParameters);

// p2p helper functions
long WlanConnect();
long StartDeviceInP2P();
long P2PConfiguration();

// AP helper functions
int ConfigureMode(int iMode);


#ifdef __cplusplus
}
#endif

#endif /* APP_LINK_H_ */
