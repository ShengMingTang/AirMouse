// Standard includes
#include <app_global_variables.h>
#include <app_simplelink_config.h>
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
#include "gpio_if.h"
#include "uart_if.h"
#include "common.h"

#include "smartconfig.h"
#include "pinmux.h"

// custom includes
#include "app_storage.h"

#define USE_BUFFER

#if defined(USE_BUFFER)
static char staticPushFile[512];
static char staticPullFile[512];
#endif

//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- Start
//*****************************************************************************
OsiSyncObj_t pushSync; // control *push* object
OsiSyncObj_t pullSync; // control *pull* object
//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- End
//*****************************************************************************

void storageControllerTask(void *pvParameters)
{
    long lRetVal = -1;
    StorageMsg_t *pMsg = (StorageMsg_t*)pvParameters;
    
    if(!pMsg){
        ERR_PRINT(pMsg);
        LOOP_FOREVER();
    }

    // while(osi_SyncObjWait(pMsg->pSync, OSI_WAIT_FOREVER) == OSI_OK){
    while(osi_SyncObjWait(pMsg->pSync, OSI_WAIT_FOREVER) == OSI_OK){
        osi_LockObjLock(pMsg->pLock, OSI_WAIT_FOREVER);

        lRetVal = 0;

        switch (pMsg->op)
        {
            case STORAGE_OP_OPEN_WRITE:
                // msg should be filename
                
                lRetVal = appCreateFile(pMsg);
                if(lRetVal < 0)
                    ERR_PRINT(lRetVal);
                else
                    lRetVal = appOpenFile(pMsg, FS_MODE_OPEN_WRITE);

                UART_PRINT("Push to ");
                CHARS_PRINT(pMsg->pFile->filename, strlen(pMsg->pFile->filename));
                UART_PRINT(" Starts\n\r");
                break;
            case STORAGE_OP_OPEN_READ:
                // msg should be filename
                
                lRetVal = appCreateFile(pMsg);
                if(lRetVal < 0)
                    ERR_PRINT(lRetVal);
                else
                    lRetVal = appOpenFile(pMsg, FS_MODE_OPEN_READ);
                
                break;
            case STORAGE_OP_WRITE:
                // msg should be base64 data
                
                lRetVal = appWriteToFile(pMsg);
                
                UART_PRINT("Uploading\n\r");
                CHARS_PRINT(pMsg->msg, pMsg->msgLen);
                UART_PRINT("\n\r");
                break;
            case STORAGE_OP_READ:
                // msg should be empty and is data being read
                // msgLen should be bytes intended to read 
                // and modified to bytes actaully read after this call
                
                lRetVal = appReadFromeFile(pMsg);

                break;
            case STORAGE_OP_CLOSE:
                // msg should be filename
                
                lRetVal = appCloseFile(pMsg);
                
                UART_PRINT("Push to ");
                CHARS_PRINT(pMsg->pFile->filename, strlen(pMsg->pFile->filename));
                UART_PRINT(" Ends\n\r");
                
                break;
            default:
                break;
        }
        if(lRetVal < 0){
            ERR_PRINT(lRetVal);
        }
        osi_LockObjUnlock(pMsg->pLock);
    }

    UART_PRINT("Push controller died...\n\r");
    LOOP_FOREVER();
}

long appCreateFile(StorageMsg_t *pMsg)
{
    long lRetVal = -1;

    strcpy(pMsg->pFile->filename, pMsg->msg);

#if defined(USE_BUFFER)
    UART_PRINT("%s created\n\r", pMsg->msg);
    lRetVal = 0;
#else

    lRetVal = sl_FsOpen((unsigned char*)pMsg->msg,
        FS_MODE_OPEN_CREATE(OPEN_FILE_SIZE, 
            // _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_WRITE),
            _FS_FILE_OPEN_FLAG_COMMIT),
        &(pMsg->pFile->token),
        &(pMsg->pFile->fileHandle)
    );
    if(lRetVal < 0){
        UART_PRINT("%s already created\n\r", pMsg->msg);
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_ALREADY_EXIST);
    }
    else{
        UART_PRINT("%s created\n\r", pMsg->msg);
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);
    }
#endif

    return lRetVal;
}
long appOpenFile(StorageMsg_t *pMsg, long mode)
{
    long lRetVal = -1;

#if defined(USE_BUFFER)
    lRetVal = 0;
#else    
    lRetVal = sl_FsOpen((const unsigned char*)pMsg->msg,
        mode,
        &(pMsg->pFile->token),
        &(pMsg->pFile->fileHandle)
    );
    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);
    }
#endif

    pMsg->pFile->offset = 0;

    return lRetVal;
}
long appWriteToFile(StorageMsg_t *pMsg)
{
    long lRetVal = -1;

#if defined(USE_BUFFER)
    memcpy(staticPushFile + (pMsg->pFile->offset), pMsg->msg, pMsg->msgLen);
    lRetVal = 0;
#else
    lRetVal = sl_FsWrite(pMsg->pFile->fileHandle,
        pMsg->pFile->offset,
        (unsigned char*)pMsg->msg, pMsg->msgLen
    );
#endif

    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        ASSERT_ON_ERROR(lRetVal);
    }
    else{
        pMsg->pFile->offset += pMsg->msgLen;
    }

    return lRetVal;
}
long appReadFromeFile(StorageMsg_t *pMsg)
{
    long lRetVal = -1;

#if defined(USE_BUFFER)
    memcpy(pMsg->msg, staticPushFile + pMsg->pFile->offset, pMsg->msgLen);
    lRetVal = 0;
#else
    lRetVal = sl_FsRead(pMsg->pFile->fileHandle,
        pMsg->pFile->offset,
        (unsigned char*)pMsg->msg, pMsg->msgLen
    );
#endif

    if(lRetVal < 0){
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_READ_FAILED);
    }
    else{
        pMsg->pFile->offset += pMsg->msgLen;
    }

    return lRetVal;
}
long appCloseFile(StorageMsg_t *pMsg)
{
    long lRetVal = -1;
    SlFsFileInfo_t info;

#if defined(USE_BUFFER)
    UART_PRINT("Test Success\n\r");
    CHARS_PRINT(staticPushFile, pMsg->pFile->offset);
    UART_PRINT("\n\r");
    lRetVal = 0;
#else
    lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        ASSERT_ON_ERROR(FILE_CLOSE_ERROR);
    }
    else{
        lRetVal = sl_FsGetInfo((const unsigned char*)pMsg->pFile->filename, pMsg->pFile->token, &info);
        if(lRetVal < 0){
            ERR_PRINT(lRetVal);
            ASSERT_ON_ERROR(lRetVal);
        }
        else{
            UART_PRINT("Info: flags=%d, fileLen=%d, allocated=%d\n\r",
                info.flags, info.FileLen, info.AllocatedLen
            );
        }
    }
#endif
    return lRetVal;
}
