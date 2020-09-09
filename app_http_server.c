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
//#include "gpio_if.h"
#include "uart_if.h"
#include "common.h"

// #include "smartconfig.h"
// #include "pinmux.h"

// custom includes
#include "app_defines.h"
#include "app_global_variables.h"
#include "app_simplelink_config.h"
#include "app_storage.h"

//*****************************************************************************
//                 EXTERN -- Start
//*****************************************************************************
//*****************************************************************************
//                 EXTERN -- End
//*****************************************************************************


//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- Start
//*****************************************************************************
OsiSyncObj_t httpKickStarter;
//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- End
//*****************************************************************************
void HTTPServerInit()
{
    long lRetVal = -1;
    lRetVal = osi_SyncObjCreate(&httpKickStarter);
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
}
//****************************************************************************
//
//! \brief                     Handles HTTP Server Task
//! \brief                     This task only handles server's job,
//! \brief                     connection should be handled by others
//!
//! \param[in]                  pvParameters is the data passed to the Task
//!
//! \return                        None
//
//****************************************************************************
void HTTPServerTask(void *pvParameters)
{
    long lRetVal = -1;

    while(osi_SyncObjWait(httpKickStarter, OSI_WAIT_FOREVER) == OSI_OK){
        //Read Device Mode Configuration
        ReadDeviceConfiguration();

        //Stop Internal HTTP Server
        lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
        if(lRetVal < 0)
        {
            ERR_PRINT(lRetVal);
            LOOP_FOREVER();
        }

        //Start Internal HTTP Server
        lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
        if(lRetVal < 0)
        {
            ERR_PRINT(lRetVal);
            LOOP_FOREVER();
        }
    }

    UART_PRINT("Http server task died...\n\r");
    while(1) {}
}
//*****************************************************************************
//
//! This function gets triggered when HTTP Server receives Application
//! defined GET and POST HTTP Tokens.
//!
//! \param pHttpServerEvent Pointer indicating http server event
//! \param pHttpServerResponse Pointer indicating http server response
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{    
    if(!pSlHttpServerEvent || !pSlHttpServerResponse)
    {
        return;
    }

    switch (pSlHttpServerEvent->Event)
    {
        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
        {
          processGetToken(pSlHttpServerEvent, pSlHttpServerResponse);
        }
        break;

        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
        {
          processPostToken(pSlHttpServerEvent, pSlHttpServerResponse);
        }
          break;
        default:
          break;
    }
}
