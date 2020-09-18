// Standard includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// Simplelink includes
#include "simplelink.h"
#include "netcfg.h"
//driverlib includes
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "utils.h"
#include "pin.h"
#include "uart.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
// common interface includes
#include "gpio_if.h"
#include "uart_if.h"
#include "common.h"
#include "smartconfig.h"
#include "pinmux.h"
// FreeRTOS includes
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
// custom includes
#include "app_simplelink_config.h"
#include "app_p2p.h"

extern volatile unsigned long  g_ulStatus; //SimpleLink Status
char g_p2p_dev[MAXIMAL_SSID_LENGTH + 1];

//****************************************************************************
//
//! \brief #could be more simplified
//!        
//!
//! \return
//
//****************************************************************************
void P2PManagerTask(void *pvParameters)
{
    long lRetVal = -1;

    UART_PRINT("Scan Wi-FI direct device in your handheld device\n\r");
    
    /* Connect to p2p device (keep trying) */
    do{
        // Initializing the CC3200 device
        lRetVal = StartDeviceInP2P();
        if(lRetVal < 0)
        {
            UART_PRINT("Start Device in P2P mode failed \n\r");
            LOOP_FOREVER();
        }

        lRetVal = P2PConfiguration();
        if(lRetVal < 0)
        {
            UART_PRINT("Setting P2P configuration failed\n\r");
            LOOP_FOREVER();
        }
        UART_PRINT("Connecting...\n\r");
        lRetVal = WlanConnect();
        if(lRetVal < 0){ // connection failed
            UART_PRINT("Connection failed, sleep for 1000 ms\n\r");
            osi_Sleep(1000);
        }
        else{
            UART_PRINT("Connected\n\r");
            
            if(DisplayIP() < 0){
                UART_PRINT("Get IP address failed \n\r");
                LOOP_FOREVER();
            }

            // @@ do something

            while(IS_CONNECTED(g_ulStatus)){ // loop until disconnected
                taskYIELD();
            }
        }
    }while(1);
}

//****************************************************************************
//
//!  \brief Connecting to a WLAN Access point
//!
//!   This function connects to the required AP (SSID_NAME) with Security
//!   parameters specified in the form of macros at the top of this file
//!
//!   \param[in]              None
//!
//!   \return                 None
//!
//!   \warning    If the WLAN connection fails or we don't acquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
long WlanConnect()
{
    SlSecParams_t secParams = {0};
    long lRetVal = 0;

    secParams.Key = (signed char *)P2P_SECURITY_KEY;
    secParams.KeyLen = strlen(P2P_SECURITY_KEY);
    secParams.Type = P2P_SECURITY_TYPE;

    lRetVal = sl_WlanConnect((signed char *)P2P_REMOTE_DEVICE,
                            strlen((const char *)P2P_REMOTE_DEVICE), 0,
                            &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Wait till Device acquired an IP in P2P mode
    while(! IS_P2P_REQ_RCVD(g_ulStatus))

    {
#ifndef SL_PLATFORM_MULTI_THREADED
        _SlNonOsMainLoopTask();
#endif
    }

    // Connect with the device requesting the connection
    lRetVal = sl_WlanConnect((signed char *)g_p2p_dev,
                           strlen((const char *)g_p2p_dev),
                           0, &secParams, 0);
    ASSERT_ON_ERROR(lRetVal);

#ifdef P2P_ROLE_TYPE_NEGOTIATE
    while(! IS_IP_ACQUIRED(g_ulStatus))
#else
    while(! IS_IP_LEASED(g_ulStatus))
#endif
    {
#ifndef SL_PLATFORM_MULTI_THREADED
        _SlNonOsMainLoopTask();
#endif
        if(IS_CONNECT_FAILED(g_ulStatus))
        {
            // Error, connection is failed
            ASSERT_ON_ERROR(NETWORK_CONNECTION_FAILED);
        }
    }

    return SUCCESS;
}

//*****************************************************************************
//
//! Check the device mode and switch to P2P mode
//! restart the NWP to activate P2P mode
//!
//! \param  None
//!
//! \return status code - Success:0, Failure:-ve
//
//*****************************************************************************
long StartDeviceInP2P()
{
    long lRetVal = -1;
    // Reset CC3200 Network State Machine
    InitializeAppVariables();

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of application
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
            UART_PRINT("Failed to configure the device in its default state \n\r");

        LOOP_FOREVER();
    }

    UART_PRINT("Device is configured in default state \n\r");

    //
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //

    lRetVal = sl_Start(NULL,NULL,NULL);
    ASSERT_ON_ERROR(lRetVal);

    if(lRetVal != ROLE_P2P)
    {
        lRetVal = sl_WlanSetMode(ROLE_P2P);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);

        // reset the Status bits
        CLR_STATUS_BIT_ALL(g_ulStatus);

        lRetVal = sl_Start(NULL,NULL,NULL);
        if(lRetVal < 0 || lRetVal != ROLE_P2P)
        {
            ASSERT_ON_ERROR(P2P_MODE_START_FAILED);
        }
        else
        {
            UART_PRINT("Started SimpleLink Device: P2P Mode\n\r");
            return SUCCESS;
        }
    }

    return SUCCESS;
}

//*****************************************************************************
//
//! Set all P2P Configuration to device
//!
//! \param  None
//!
//! \return status code - Success:0, Failure:-ve
//
//*****************************************************************************
long P2PConfiguration()
{
    unsigned char ucP2PParam[4];
    long lRetVal = -1;

    // Set any p2p option (SL_CONNECTION_POLICY(0,0,0,any_p2p,0)) to connect to
    // first available p2p device
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(1,0,0,0,0),NULL,0);
    ASSERT_ON_ERROR(lRetVal);

    // Set the negotiation role (SL_P2P_ROLE_NEGOTIATE).
    // CC3200 will negotiate with remote device GO/client mode.
    // Other valid options are:
    //             - SL_P2P_ROLE_GROUP_OWNER
    //             - SL_P2P_ROLE_CLIENT
    lRetVal = sl_WlanPolicySet(SL_POLICY_P2P, SL_P2P_POLICY(SL_P2P_ROLE_NEGOTIATE,
                                   SL_P2P_NEG_INITIATOR_ACTIVE),NULL,0);
    ASSERT_ON_ERROR(lRetVal);

    // Set P2P Device name
    lRetVal = sl_NetAppSet(SL_NET_APP_DEVICE_CONFIG_ID, \
                             NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN,
                           strlen(P2P_DEVICE_NAME), (unsigned char *)P2P_DEVICE_NAME);
    ASSERT_ON_ERROR(lRetVal);

    // Set P2P device type
    lRetVal = sl_WlanSet(SL_WLAN_CFG_P2P_PARAM_ID, WLAN_P2P_OPT_DEV_TYPE,
                         strlen(P2P_CONFIG_VALUE), (unsigned char*)P2P_CONFIG_VALUE);
    ASSERT_ON_ERROR(lRetVal);

    // setting P2P channel parameters
    ucP2PParam[0] = LISENING_CHANNEL;
    ucP2PParam[1] = REGULATORY_CLASS;
    ucP2PParam[2] = OPERATING_CHANNEL;
    ucP2PParam[3] = REGULATORY_CLASS;

    // Set P2P Device listen and open channel valid channels are 1/6/11
    lRetVal = sl_WlanSet(SL_WLAN_CFG_P2P_PARAM_ID, WLAN_P2P_OPT_CHANNEL_N_REGS,
                sizeof(ucP2PParam), ucP2PParam);
    ASSERT_ON_ERROR(lRetVal);

    // Restart as P2P device
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);

    lRetVal = sl_Start(NULL,NULL,NULL);
    if(lRetVal < 0 || lRetVal != ROLE_P2P)
    {
        ASSERT_ON_ERROR(P2P_CONFIG_FAILED);
    }
    else
    {
        UART_PRINT("Connect to %s \n\r",P2P_DEVICE_NAME);
    }

    return SUCCESS;
}
