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

//Free_rtos/ti-rtos includes
#include "osi.h"

// common interface includes
#include "gpio_if.h"
#include "uart_if.h"
#include "common.h"

#include "smartconfig.h"
#include "pinmux.h"

// custom includes
#include "app_defines.h"
#include "app_global_variables.h"
#include "app_simplelink_config.h"
#include "app_ap.h"
#include "ftp/ftp_server.h"

extern OsiSyncObj_t httpKickStarter;
extern SemaphoreHandle_t semFtpKickStarter;
extern SemaphoreHandle_t semHidKickStarter;

//****************************************************************************
//
//! Confgiures the mode in which the device will work
//!
//! \param iMode is the current mode of the device
//!
//! This function
//!    1. prompt user for desired configuration and accordingly configure the
//!          networking mode(STA or AP).
//!       2. also give the user the option to configure the ssid name in case of
//!       AP mode.
//!
//! \return sl_start return value(int).
//
//****************************************************************************
static int ConfigureMode(int iMode)
{
    long   lRetVal = -1;


    lRetVal = sl_WlanSetMode(ROLE_AP);
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, strlen(AP_SSID),
                            (unsigned char*)AP_SSID);
    ASSERT_ON_ERROR(lRetVal);

    UART_PRINT("Device is configured in AP mode\n\r");

    /* Restart Network processor */
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);

    // reset status bits
    CLR_STATUS_BIT_ALL(g_ulStatus);

    return sl_Start(NULL,NULL,NULL);
}

void APTask(void *pvParameters)
{
    unsigned char ucDHCP;
    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    SlNetCfgIpV4Args_t ipV4 = {0};
    long lRetVal = -1;

    InitializeAppVariables();

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
            UART_PRINT("Failed to configure the device in its default state\n\r");

        LOOP_FOREVER();
    }
    UART_PRINT("Device is configured in default state \n\r");

    //
    // Asumption is that the device is configured in station mode already
    // and it is in its default state
    //
    lRetVal = sl_Start(NULL,NULL,NULL);

    if (lRetVal < 0)
    {
        UART_PRINT("Failed to start the device \n\r");
        LOOP_FOREVER();
    }

    UART_PRINT("Device started as STATION \n\r");
    
    //
    // Configure the networking mode and ssid name(for AP mode)
    //
    if(lRetVal != ROLE_AP)
    {
        if(ConfigureMode(lRetVal) != ROLE_AP)
        {
            UART_PRINT("Unable to set AP mode, exiting Application...\n\r");
            sl_Stop(SL_STOP_TIMEOUT);
            LOOP_FOREVER();
        }
    }

    //looping till ip is acquired
    while(!IS_IP_ACQUIRED(g_ulStatus))
    {
        taskYIELD();
    }

    while(1){
        // get network configuration
        lRetVal = sl_NetCfgGet(SL_IPV4_AP_P2P_GO_GET_INFO,&ucDHCP,&len,
                                (unsigned char *)&ipV4);
        if (lRetVal < 0)
        {
            UART_PRINT("Failed to get network configuration \n\r");
            LOOP_FOREVER();
        }
        
        UART_PRINT("Connect a client to Device...\n\r");
        
        while(!IS_IP_LEASED(g_ulStatus)){ // loop until connected
            taskYIELD();
        }

        // xSemaphoreGive(semFtpKickStarter);
        xSemaphoreGive(semHidKickStarter);
        // osi_SyncObjSignal(&httpKickStarter);

        while(IS_IP_LEASED(g_ulStatus)){ // loop until disconnected
            taskYIELD();
        }
    }

    /*  
    // revert to STA mode
    lRetVal = sl_WlanSetMode(ROLE_STA);
    if(lRetVal < 0)
    {
      ERR_PRINT(lRetVal);
      LOOP_FOREVER();
    }
    */
}
