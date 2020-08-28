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

extern OsiSyncObj_t httpKickStarter;

void APTask(void *pvParameters)
{
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
  
    memset(g_ucSSID,'\0',AP_SSID_LEN_MAX);
    
    //Read Device Mode Configuration
    ReadDeviceConfiguration();

    //Connect to Network
    lRetVal = ConnectToNetwork();
    
    osi_SyncObjSignal(&httpKickStarter);

    //Handle Async Events
    while(1)
    {
         
    }
}

//****************************************************************************
//
//!    \brief Connects to the Network in AP or STA Mode - If ForceAP Jumper is
//!                                             Placed, Force it to AP mode
//!
//! \return                        0 on success else error code
//
//****************************************************************************
long ConnectToNetwork()
{
    char ucAPSSID[32];
    unsigned short len, config_opt;
    long lRetVal = -1;

    // staring simplelink
    g_uiSimplelinkRole =  sl_Start(NULL,NULL,NULL);

    // Device is not in STA mode and Force AP Jumper is not Connected 
    //- Switch to STA mode
    if(g_uiSimplelinkRole != ROLE_STA && g_uiDeviceModeConfig == ROLE_STA )
    {
        //Switch to STA Mode
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);
        
        lRetVal = sl_Stop(SL_STOP_TIMEOUT);
        
        g_usMCNetworkUstate = 0;
        g_uiSimplelinkRole =  sl_Start(NULL,NULL,NULL);
    }

    //Device is not in AP mode and Force AP Jumper is Connected - 
    //Switch to AP mode
    if(g_uiSimplelinkRole != ROLE_AP && g_uiDeviceModeConfig == ROLE_AP )
    {
         //Switch to AP Mode
        lRetVal = sl_WlanSetMode(ROLE_AP);
        ASSERT_ON_ERROR(lRetVal);
        
        lRetVal = sl_Stop(SL_STOP_TIMEOUT);
        
        g_usMCNetworkUstate = 0;
        g_uiSimplelinkRole =  sl_Start(NULL,NULL,NULL);
    }

    //No Mode Change Required
    if(g_uiSimplelinkRole == ROLE_AP)
    {
       //waiting for the AP to acquire IP address from Internal DHCP Server
       while(!IS_IP_ACQUIRED(g_ulStatus))
       {

       }

       char iCount=0;
       //Read the AP SSID
       memset(ucAPSSID,'\0',AP_SSID_LEN_MAX);
       len = AP_SSID_LEN_MAX;
       config_opt = WLAN_AP_OPT_SSID;
       lRetVal = sl_WlanGet(SL_WLAN_CFG_AP_ID, &config_opt , &len,
                                              (unsigned char*) ucAPSSID);
        ASSERT_ON_ERROR(lRetVal);
        
       Report("\n\rDevice is in AP Mode, Please Connect to AP [%s] and"
          "type [mysimplelink.net] in the browser \n\r",ucAPSSID);
       
       //Blink LED 3 times to Indicate AP Mode
       for(iCount=0;iCount<3;iCount++)
       {
           //Turn RED LED On
           GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           osi_Sleep(400);
           
           //Turn RED LED Off
           GPIO_IF_LedOff(MCU_RED_LED_GPIO);
           osi_Sleep(400);
       }

    }
    else
    {
		//waiting for the device to Auto Connect
		while ( (!IS_IP_ACQUIRED(g_ulStatus))&&
			   g_ucConnectTimeout < AUTO_CONNECTION_TIMEOUT_COUNT)
		{
			//Turn RED LED On
			GPIO_IF_LedOn(MCU_RED_LED_GPIO);
			osi_Sleep(50);

			//Turn RED LED Off
			GPIO_IF_LedOff(MCU_RED_LED_GPIO);
			osi_Sleep(50);

			g_ucConnectTimeout++;
		}
		//Couldn't connect Using Auto Profile
		if(g_ucConnectTimeout == AUTO_CONNECTION_TIMEOUT_COUNT)
		{
			//Blink Red LED to Indicate Connection Error
			GPIO_IF_LedOn(MCU_RED_LED_GPIO);

			CLR_STATUS_BIT_ALL(g_ulStatus);

			Report("Use Smart Config Application to configure the device.\n\r");
			//Connect Using Smart Config
			lRetVal = SmartConfigConnect();
			ASSERT_ON_ERROR(lRetVal);

			//Waiting for the device to Auto Connect
			while(!IS_IP_ACQUIRED(g_ulStatus))
			{
				MAP_UtilsDelay(500);
			}

		}
    //Turn RED LED Off
    GPIO_IF_LedOff(MCU_RED_LED_GPIO);
    UART_PRINT("\n\rDevice is in STA Mode, Connect to the AP[%s] and type"
          "IP address [%d.%d.%d.%d] in the browser \n\r",g_ucConnectionSSID,
          SL_IPV4_BYTE(g_ulDeviceIp,3),SL_IPV4_BYTE(g_ulDeviceIp,2),
          SL_IPV4_BYTE(g_ulDeviceIp,1),SL_IPV4_BYTE(g_ulDeviceIp,0));

    }

    return SUCCESS;
}