// Standard includes
#include <string.h>
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
extern OsiLockObj_t pushLock;
extern OsiSyncObj_t pushSync;
extern StorageFile_t pushFile;
extern StorageMsg_t pushMsg;

extern OsiLockObj_t pullLock;
extern OsiSyncObj_t pullSync;
extern StorageFile_t pullFile;
extern StorageMsg_t pullMsg;
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
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent,
                               SlHttpServerResponse_t *pSlHttpServerResponse)
{
    unsigned char strLenVal = 0;
    int i;
    
    if(!pSlHttpServerEvent || !pSlHttpServerResponse)
    {
        return;
    }

    switch (pSlHttpServerEvent->Event)
    {
        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
        {
          unsigned char *ptr;

          ptr = pSlHttpServerResponse->ResponseData.token_value.data;
          pSlHttpServerResponse->ResponseData.token_value.len = 0;
          if(IS_TOKEN_MATCH(pSlHttpServerEvent->EventData.httpTokenName.data, "__SL_G_UXX"))
          {
            strLenVal = strlen((const char*)"Shengming.tang");
            memcpy(ptr, "Shengming.tang", strLenVal);
            ptr += strLenVal;
            pSlHttpServerResponse->ResponseData.token_value.len = strLenVal;
            *ptr = '\0';
          }
          // always print
          for(i = 0; i < pSlHttpServerEvent->EventData.httpTokenName.len; i++){
            UART_PRINT("%c", pSlHttpServerEvent->EventData.httpTokenName.data[i]);
          }
          UART_PRINT("\n\r");
        }
        break;

        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
        {
          unsigned char *ptrName = pSlHttpServerEvent->EventData.httpPostData.token_name.data;
          long lenName = pSlHttpServerEvent->EventData.httpPostData.token_name.len;
          unsigned char *ptrValue = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
          long lenValue = pSlHttpServerEvent->EventData.httpPostData.token_value.len;

          // print user token
          UART_PRINT("POST: ");
          CHARS_PRINT(pSlHttpServerEvent->EventData.httpPostData.token_name.data, pSlHttpServerEvent->EventData.httpPostData.token_name.len);
          UART_PRINT("=");
          CHARS_PRINT(pSlHttpServerEvent->EventData.httpPostData.token_value.data, pSlHttpServerEvent->EventData.httpPostData.token_value.len);
          UART_PRINT("\n\r");
          
          if(IS_TOKEN_MATCH(ptrName, PUSH_START_TOKEN)){ // push starts
            // atomic
            osi_LockObjLock(&pushLock, OSI_WAIT_FOREVER);
            memcpy(pushMsg.msg, ptrValue, lenValue);
            (pushMsg.msg)[lenValue] = '\0';
            pushMsg.msgLen = lenValue;
            pushMsg.op = STORAGE_OP_OPEN_WRITE;
            osi_LockObjUnlock(&pushLock);
            // atomic
            osi_SyncObjSignalFromISR(&pushSync);
          }
          else if(IS_TOKEN_MATCH(ptrName, PUSH_END_TOKEN)){
            // atomic
            osi_LockObjLock(&pushLock, OSI_WAIT_FOREVER);
            memcpy(pushMsg.msg, ptrValue, lenValue);
            (pushMsg.msg)[lenValue] = '\0';
            pushMsg.msgLen = lenValue;
            pushMsg.op = STORAGE_OP_CLOSE;
            osi_LockObjUnlock(&pushLock);
            // atomic
            osi_SyncObjSignalFromISR(&pushSync);
          }
          else if(IS_TOKEN_MATCH(ptrName, PUSH_PROGRESS_TOKEN)){ // pushing
            // atomic
            osi_LockObjLock(&pushLock, OSI_WAIT_FOREVER);
            memcpy(pushMsg.msg, ptrValue, lenValue);
            pushMsg.msgLen = lenValue;
            pushMsg.op = STORAGE_OP_WRITE;
            osi_LockObjUnlock(&pushLock);
            // atomic
            osi_SyncObjSignalFromISR(&pushSync);
          }
        }
          break;
        default:
          break;
    }
}
