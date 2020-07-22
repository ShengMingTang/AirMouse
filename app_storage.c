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

#include "sdhost.h"

// FatFS includes
#include "ff.h"
//Free_rtos/ti-rtos includes
#include "osi.h"

// common interface includes
#include "gpio_if.h"
#include "uart_if.h"
#include "common.h"

#include "smartconfig.h"
#include "pinmux.h"

// custom includes
#include "app_global_variables.h"
#include "app_simplelink_config.h"
#include "app_storage.h"

#if defined(USE_BUFFER)
static char staticPushFile[512];
static char staticPullFile[512] = "SGVsbG8sV29ybGQuSSdtU2hlbmcgbWluZyBUYW5n";
#endif

static FATFS fs;

//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- Start
//*****************************************************************************
OsiLockObj_t pushLock;
OsiSyncObj_t pushSync;
StorageFile_t pushFile;
StorageMsg_t pushMsg;
OsiLockObj_t pullLock;
OsiSyncObj_t pullSync;
StorageFile_t pullFile;
StorageMsg_t pullMsg;
//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- End
//*****************************************************************************

long storageFatFsInit()
{
    FRESULT res = FR_NOT_READY;
    DIR dir;

    while(res == FR_NOT_READY){
        res = f_mount(&fs, "0", FATFS_MOUNT_OPT);
    }

    res = f_opendir(&dir, "/");
    IS_EXPECTED_VALUE(res, FR_OK);

    listDirectory(&dir);

    return 0;
}

void storageControllerTask(void *pvParameters)
{
    long lRetVal = -1;
    StorageMsg_t *pMsg = (StorageMsg_t*)pvParameters;
    FRESULT res;

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
                #if defined(USE_BUFFER) || defined(USE_SL_FS)
                lRetVal = appOpenFile(pMsg, FS_MODE_OPEN_WRITE);
                #else
                lRetVal = appOpenFile(pMsg, FA_CREATE_ALWAYS|FA_WRITE);
                #endif
                
                UART_PRINT("Push to ");
                CHARS_PRINT(pMsg->pFile->filename, strlen(pMsg->pFile->filename));
                UART_PRINT(" Starts\n\r");
                break;
            case STORAGE_OP_OPEN_READ:
                // msg should be filename
                #if defined(USE_BUFFER) || defined(USE_SL_FS)
                lRetVal = appOpenFile(pMsg, FS_MODE_OPEN_READ);
                #else
                lRetVal = appOpenFile(pMsg, FA_READ);
                #endif

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
                
                DIR dir;
                res = f_opendir(&dir, "/");
                IS_EXPECTED_VALUE(res, FR_OK);
                listDirectory(&dir);

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

void listDirectory(DIR *dir)
{
    FILINFO fno;
    FRESULT res;
    unsigned long ulSize;
    tBoolean bIsInKB;
        
    printf("Start listing\n\r");
    for(;;){
        res = f_readdir(dir, &fno);
        if(res != FR_OK || fno.fname[0] == 0){ // break on error
            break;
        }
        ulSize = fno.fsize;
        bIsInKB = false;
        if(ulSize > 1024)
        {
            ulSize = ulSize/1024;
            bIsInKB = true;
        }
        printf("->%-15s%5d %-2s    %-5s\n\r",fno.fname,ulSize,\
                (bIsInKB == true)?"KB":"B",(fno.fattrib&AM_DIR)?"Dir":"File");
    }
    printf("End listing\n\r");
}
long appCreateFile(StorageMsg_t *pMsg)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal =  -1;
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
#else
    FRESULT res;
    strcpy(pMsg->pFile->filename, pMsg->msg);
    res = f_open(&(pMsg->pFile->fileHandle), pMsg->pFile->filename, FA_CREATE_ALWAYS | FA_WRITE);
    IS_EXPECTED_VALUE(res, FR_OK);
    return res;
#endif
}
long appOpenFile(StorageMsg_t *pMsg, long mode)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
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
    return lRetVal;
#else
    FRESULT res;
    res = f_open(&(pMsg->pFile->fileHandle), pMsg->msg, mode);
    IS_EXPECTED_VALUE(res, FR_OK);
    pMsg->pFile->offset = 0;
    return res;
#endif
}
long appWriteToFile(StorageMsg_t *pMsg)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal = -1;
    memcpy(staticPushFile + (pMsg->pFile->offset), pMsg->msg, pMsg->msgLen);
    #if defined(USE_BUFFER)
    lRetVal = 0;
    #else
    lRetVal = sl_FsWrite(pMsg->pFile->fileHandle,
        pMsg->pFile->offset,
        (unsigned char*)pMsg->msg, pMsg->msgLen
    );
    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        ASSERT_ON_ERROR(lRetVal);
    }
    else{
        pMsg->pFile->offset += pMsg->msgLen;
    }
    #endif
    return lRetVal;
#else
    FRESULT res;
    UINT size;
    res = f_write(&(pMsg->pFile->fileHandle), pMsg->msg, pMsg->msgLen, &size);
    IS_EXPECTED_VALUE(res, FR_OK);
    return res;
#endif

}
long appReadFromeFile(StorageMsg_t *pMsg)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal = -1;
    memcpy(pMsg->msg, staticPushFile + pMsg->pFile->offset, pMsg->msgLen);
    #if defined(USE_BUFFER)
    lRetVal = 0;
    pMsg->pFile->offset += pMsg->msgLen;
    #else
    lRetVal = sl_FsRead(pMsg->pFile->fileHandle,
        pMsg->pFile->offset,
        (unsigned char*)pMsg->msg, pMsg->msgLen
    );
    if(lRetVal < 0){
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_READ_FAILED);
    }
    else{
        pMsg->pFile->offset += pMsg->msgLen;
    }
    #endif
    return lRetVal;
#else
    FRESULT res;
    UINT size = pMsg->msgLen;
    res = f_read(&(pMsg->pFile->fileHandle), pMsg->msg, size, &(pMsg->msgLen));
    IS_EXPECTED_VALUE(res, FR_OK);
    return res;
#endif
}
long appCloseFile(StorageMsg_t *pMsg)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal = -1;
    #if defined(USE_BUFFER)
    UART_PRINT("Test Success\n\r");
    CHARS_PRINT(staticPushFile, pMsg->pFile->offset);
    UART_PRINT("\n\r");
    lRetVal = 0;
    #else
    SlFsFileInfo_t info;
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
#else
    FRESULT res;
    res = f_close(&(pMsg->pFile->fileHandle));
    IS_EXPECTED_VALUE(res, FR_OK);
    return res;
#endif
}
