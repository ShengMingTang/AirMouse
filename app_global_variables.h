#ifndef APP_GLOBAL_VARIABLES_H_
#define APP_GLOBAL_VARIABLES_H_

// Simplelink includes
#include "simplelink.h"
#include "netcfg.h"

// common interface includes
#include "common.h"

// app includes
#include "app_defines.h"

//*****************************************************************************
//                 EXTERN GLOBAL VARIABLES -- Start
//*****************************************************************************

extern volatile unsigned long  g_ulStatus;//SimpleLink Status
extern unsigned long  g_ulPingPacketsRecv; //Number of Ping Packets received
extern unsigned long  g_ulDeviceIp;
extern unsigned long  g_ulStaIp;
extern unsigned long  g_ulGatewayIP; //Network Gateway IP address
extern unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
extern unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
extern int g_iSimplelinkRole;
extern unsigned char g_ucSSID[AP_SSID_LEN_MAX];

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// p2p
extern char g_p2p_dev[MAXIMAL_SSID_LENGTH + 1];

// osi

//*****************************************************************************
//                 EXTERN GLOBAL VARIABLES -- End
//*****************************************************************************

//*****************************************************************************
//                 Variable related to Connection status -- Start
//*****************************************************************************
extern volatile unsigned short g_usMCNetworkUstate;

extern int g_uiSimplelinkRole;
extern unsigned int g_uiDeviceModeConfig; //default is STA mode
extern volatile unsigned char g_ucConnectTimeout;
//*****************************************************************************
//                 Variable related to Connection status -- Start
//*****************************************************************************

#endif /* APP_GLOBAL_VARIABLES_H_ */
