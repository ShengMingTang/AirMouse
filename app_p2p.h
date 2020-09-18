#ifndef APP_P2P_H_
#define APP_P2P_H_

// P2P defines
#define P2P_REMOTE_DEVICE   "remote-p2p-device"
#define P2P_DEVICE_NAME     "Air-Mouse"
#define P2P_SECURITY_TYPE   SL_SEC_TYPE_P2P_PBC
#define P2P_SECURITY_KEY    ""

#define P2P_CONFIG_VALUE    "2-0050F204-2"

#define LISENING_CHANNEL    11
#define REGULATORY_CLASS    81
#define OPERATING_CHANNEL   6

#define P2P_ROLE_TYPE_NEGOTIATE

void P2PManagerTask(void * pvParameters);

long WlanConnect();
long StartDeviceInP2P();
long P2PConfiguration();

#endif /* APP_P2P_H_ */
