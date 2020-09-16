#ifndef APP_P2P_H_
#define APP_P2P_H_

#include "main_config.h"

void P2PManagerTask(void * pvParameters);

long WlanConnect();
long StartDeviceInP2P();
long P2PConfiguration();

#endif /* APP_P2P_H_ */
