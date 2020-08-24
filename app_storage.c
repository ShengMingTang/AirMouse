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
#include "cJSON/cJSON.h"

#include "app_global_variables.h"
#include "app_simplelink_config.h"
#include "app_storage.h"

#if defined(USE_BUFFER)
static char staticPushFile[512];
static char staticPullFile[512] = "SGVsbG8sV29ybGQuSSdtU2hlbmcgbWluZyBUYW5n";
#endif

char fsStr[FS_STR_MAX_LEN];

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

OsiLockObj_t fsLock;
OsiSyncObj_t fsSync;
//*****************************************************************************
//                 SYNCHRONIZATION OBJECTS -- End
//*****************************************************************************

long storageFatFsInit()
{
    OsiReturnVal_e osiRetVal;
    long lRetVal = -1;

    // init pushMsg
    pushFile.offset = 0;
    osiRetVal = osi_LockObjCreate(&pushLock);
    IS_EXPECTED_VALUE(osiRetVal, OSI_OK);

    osiRetVal = osi_SyncObjCreate(&pushSync);
    IS_EXPECTED_VALUE(osiRetVal, OSI_OK);

    pushMsg.pLock = &pushLock;
    pushMsg.pSync = &pushSync;
    pushMsg.pFile = &pushFile;
    pushMsg.op = STORAGE_OP_INVALID;

    // mount fs
    FRESULT res = FR_NOT_READY;
    DIR dir;
    cJSON_bool overflow;

    osiRetVal = osi_SyncObjCreate(&fsSync);
    IS_EXPECTED_VALUE(osiRetVal, OSI_OK);
    osiRetVal = osi_LockObjCreate(&fsLock);
    IS_EXPECTED_VALUE(osiRetVal, OSI_OK);

    while (res == FR_NOT_READY)
    {
        res = f_mount(&fs, "0", FATFS_MOUNT_OPT);
    }

    // init fs str
    cJSON *root = listDirectoryInJson("/", 0);
    memset(fsStr, '\0', sizeof(fsStr));
    overflow = cJSON_PrintPreallocated(root, fsStr, sizeof(fsStr), 0);
    cJSON_Delete(root);
    IS_EXPECTED_VALUE(overflow, true);
    UART_PRINT("%s\n\r", fsStr);

    // create storage Push Task
    lRetVal = osi_TaskCreate(storageControllerTask, (signed char *)"pushController",
                             OSI_STACK_SIZE, &pushMsg, OOB_TASK_PRIORITY, NULL);
    if (lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }

    return 0;
}
void processGetToken(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{
    unsigned char *ptrName = pSlHttpServerEvent->EventData.httpTokenName.data;
    long lenName = pSlHttpServerEvent->EventData.httpTokenName.len;
    unsigned char *ptrValue = pSlHttpServerResponse->ResponseData.token_value.data;
    long strLenVal = 0;
    
    int offset, final;
    int i;
    // always print
    // CHARS_PRINT(pSlHttpServerEvent->EventData.httpTokenName.data, pSlHttpServerEvent->EventData.httpTokenName.len);
    // UART_PRINT("\n\r");

    // fs
    if (IS_TOKEN_MATCH(ptrName, FS_LIST_TOKEN_S)){ // list directory request
        strLenVal = strlen((const char *)" ");
        memcpy(ptrValue, " ", strLenVal);
        ptrValue += strLenVal;
        pSlHttpServerResponse->ResponseData.token_value.len = strLenVal;
        *ptrValue = '\0';
        osi_LockObjLock(&pushLock, OSI_WAIT_FOREVER);
        pushMsg.op = STORAGE_OP_LIST;
        osi_LockObjUnlock(&pushLock);
        osi_SyncObjSignalFromISR(&pushSync); // assign to push worker
    }
    else if (IS_TOKEN_MATCH(ptrName, FS_LIST_TOKEN)){ // fs in json
        offset = ptrName[9] - 'a';
        if ((offset + 1) * TRANSFER_BLOCK_SIZE >= strlen(fsStr)){
            strLenVal = strlen(fsStr) - (offset * TRANSFER_BLOCK_SIZE);
            strLenVal = strLenVal > 0 ? strLenVal : 0;
        }
        else{
            strLenVal = TRANSFER_BLOCK_SIZE;
        }
        if (strLenVal > 0){
            memcpy(ptrValue, fsStr + offset * TRANSFER_BLOCK_SIZE, strLenVal);
            ptrValue += strLenVal;
            pSlHttpServerResponse->ResponseData.token_value.len = strLenVal;
            *ptrValue = '\0';
        }
        else{
            strLenVal = strlen((const char *)" ");
            memcpy(ptrValue, " ", strLenVal);
            ptrValue += strLenVal;
            pSlHttpServerResponse->ResponseData.token_value.len = strLenVal;
            *ptrValue = '\0';
        }
    }
    else if (IS_TOKEN_MATCH(ptrName, "__SL_G_UXX")){ // test
        strLenVal = strlen((const char *)"Shengming.tang");
        memcpy(ptrValue, "Shengming.tang", strLenVal);
        ptrValue += strLenVal;
        pSlHttpServerResponse->ResponseData.token_value.len = strLenVal;
        *ptrValue = '\0';
    }
}
void processPostToken(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{
    unsigned char *ptrName = pSlHttpServerEvent->EventData.httpPostData.token_name.data;
    long lenName = pSlHttpServerEvent->EventData.httpPostData.token_name.len;
    unsigned char *ptrValue = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
    long lenValue = pSlHttpServerEvent->EventData.httpPostData.token_value.len;

    unsigned char strLenVal = 0;
    int offset, final;
    int i;
    // print user token
    // UART_PRINT("POST: ");
    // CHARS_PRINT(pSlHttpServerEvent->EventData.httpPostData.token_name.data, pSlHttpServerEvent->EventData.httpPostData.token_name.len);
    // UART_PRINT("=");
    // CHARS_PRINT(pSlHttpServerEvent->EventData.httpPostData.token_value.data, pSlHttpServerEvent->EventData.httpPostData.token_value.len);
    // UART_PRINT("\n\r");

    if (IS_TOKEN_MATCH(ptrName, PUSH_START_TOKEN))
    { // init a push
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
    else if (IS_TOKEN_MATCH(ptrName, PUSH_END_TOKEN))
    { // end a push
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
    else if (IS_TOKEN_MATCH(ptrName, PUSH_PROGRESS_TOKEN))
    { // pushing
        // atomic
        osi_LockObjLock(&pushLock, OSI_WAIT_FOREVER);
        memcpy(pushMsg.msg, ptrValue, lenValue);
        pushMsg.msgLen = lenValue;
        pushMsg.op = STORAGE_OP_WRITE;
        osi_LockObjUnlock(&pushLock);
        // atomic
        osi_SyncObjSignalFromISR(&pushSync);
    }
    // ******************************************************************
    // Push part -- End
    // ******************************************************************

    // ******************************************************************
    // Pull part -- Start
    // ******************************************************************
    else if (IS_TOKEN_MATCH(ptrName, PULL_START_TOKEN))
    { // init a pull
        // atomic
        osi_LockObjLock(&pullLock, OSI_WAIT_FOREVER);
        memcpy(pullMsg.msg, ptrValue, lenValue);
        (pullMsg.msg)[lenValue] = '\0';
        pullMsg.msgLen = lenValue;
        pullMsg.op = STORAGE_OP_OPEN_READ;
        osi_LockObjUnlock(&pullLock);
        // atomic
        osi_SyncObjSignalFromISR(&pullSync);
    }
    else if (IS_TOKEN_MATCH(ptrName, PULL_END_TOKEN))
    { // end a pull
        // atomic
        osi_LockObjLock(&pullLock, OSI_WAIT_FOREVER);
        memcpy(pullMsg.msg, ptrValue, lenValue);
        (pullMsg.msg)[lenValue] = '\0';
        pullMsg.msgLen = lenValue;
        pullMsg.op = STORAGE_OP_CLOSE;
        osi_LockObjUnlock(&pullLock);
        // atomic
        osi_SyncObjSignalFromISR(&pullSync);
    }
    // ******************************************************************
    // Pull part -- Start
    // ******************************************************************
}
cJSON *listDirectoryInJson(char *path, int depth)
{
    cJSON *root;
    cJSON *children;

    DIR dir;
    FILINFO fno;
    FRESULT res;
    unsigned long ulSize;
    tBoolean bIsInKB;

    char buff[64];
    int i;

    res = f_opendir(&dir, path);
    IS_EXPECTED_VALUE(res, FR_OK);

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "l", path);
    children = cJSON_CreateArray();

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0')
        { // break on error
            break;
        }
        if (fno.fattrib & AM_DIR)
        { // is a directory
            cJSON *subDir = listDirectoryInJson(fno.fname, depth + 1);
            cJSON_AddItemToArray(children, subDir);
        }
        else
        { // a file
            cJSON *file = cJSON_CreateObject();
            cJSON_AddStringToObject(file, "l", fno.fname);
            cJSON_AddItemToArray(children, file);
        }
    }
    cJSON_AddItemToObject(root, "c", children);
    
    res = f_closedir(&dir);
    IS_EXPECTED_VALUE(res, FR_OK);

    return root;
}

void storageControllerTask(void *pvParameters)
{
    long lRetVal = -1;
    StorageMsg_t *pMsg = (StorageMsg_t *)pvParameters;
    cJSON *root;
    tBoolean overflow;
    FRESULT res;

    if (!pMsg)
    {
        ERR_PRINT(pMsg);
        LOOP_FOREVER();
    }

    // while(osi_SyncObjWait(pMsg->pSync, OSI_WAIT_FOREVER) == OSI_OK){
    while (osi_SyncObjWait(pMsg->pSync, OSI_WAIT_FOREVER) == OSI_OK)
    {
        osi_LockObjLock(pMsg->pLock, OSI_WAIT_FOREVER);

        lRetVal = 0;

        switch (pMsg->op)
        {
            case STORAGE_OP_LIST:
                // init fs str
                root = listDirectoryInJson("/", 0);
                memset(fsStr, '\0', sizeof(fsStr));
                overflow = cJSON_PrintPreallocated(root, fsStr, sizeof(fsStr), 0);
                cJSON_Delete(root);
                IS_EXPECTED_VALUE(overflow, true);
                break;
            case STORAGE_OP_OPEN_WRITE:
                // pMsg->msg should be filename
                memcpy(pMsg->pFile->filename, pMsg->msg, pMsg->msgLen);
#if defined(USE_BUFFER) || defined(USE_SL_FS)
                lRetVal = appOpenFile(pMsg, FS_MODE_OPEN_WRITE);
#else
                lRetVal = appOpenFile(pMsg, FA_CREATE_ALWAYS | FA_WRITE);
#endif
                UART_PRINT("Push to ");
                CHARS_PRINT(pMsg->pFile->filename, strlen(pMsg->pFile->filename));
                UART_PRINT(" Starts\n\r");
                break;
            case STORAGE_OP_OPEN_READ:
                // pMsg->msg should be filename
                memcpy(pMsg->pFile->filename, pMsg->msg, pMsg->msgLen);
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
                // CHARS_PRINT(pMsg->msg, pMsg->msgLen);
                // UART_PRINT("\n\r");
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
        if (lRetVal < 0)
        {
            ERR_PRINT(lRetVal);
        }
        osi_LockObjUnlock(pMsg->pLock);
    }

    UART_PRINT("Push controller died...\n\r");
    LOOP_FOREVER();
}
long appCreateFile(StorageMsg_t *pMsg)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal = -1;
#if defined(USE_BUFFER)
    UART_PRINT("%s created\n\r", pMsg->msg);
    lRetVal = 0;
#else
    lRetVal = sl_FsOpen((unsigned char *)pMsg->msg,
                        FS_MODE_OPEN_CREATE(OPEN_FILE_SIZE,
                                            // _FS_FILE_OPEN_FLAG_COMMIT | _FS_FILE_PUBLIC_WRITE),
                                            _FS_FILE_OPEN_FLAG_COMMIT),
                        &(pMsg->pFile->token),
                        &(pMsg->pFile->fileHandle));
    if (lRetVal < 0)
    {
        UART_PRINT("%s already created\n\r", pMsg->msg);
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_ALREADY_EXIST);
    }
    else
    {
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
long appOpenFile(StorageMsg_t *pMsg, BYTE mode)
{
#if defined(USE_BUFFER) || defined(USE_SL_FS)
    long lRetVal = -1;
    #if defined(USE_BUFFER)
        lRetVal = 0;
    #else
        lRetVal = sl_FsOpen((const unsigned char *)pMsg->msg,
                            mode,
                            &(pMsg->pFile->token),
                            &(pMsg->pFile->fileHandle));
        if (lRetVal < 0)
        {
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
                         (unsigned char *)pMsg->msg, pMsg->msgLen);
    if (lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        ASSERT_ON_ERROR(lRetVal);
    }
    else
    {
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
                        (unsigned char *)pMsg->msg, pMsg->msgLen);
    if (lRetVal < 0)
    {
        lRetVal = sl_FsClose(pMsg->pFile->fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_READ_FAILED);
    }
    else
    {
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
    if (lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        ASSERT_ON_ERROR(FILE_CLOSE_ERROR);
    }
    else
    {
        lRetVal = sl_FsGetInfo((const unsigned char *)pMsg->pFile->filename, pMsg->pFile->token, &info);
        if (lRetVal < 0)
        {
            ERR_PRINT(lRetVal);
            ASSERT_ON_ERROR(lRetVal);
        }
        else
        {
            UART_PRINT("Info: flags=%d, fileLen=%d, allocated=%d\n\r",
                       info.flags, info.FileLen, info.AllocatedLen);
        }
    }
#endif
    return lRetVal;
#else
    FRESULT res = FR_NOT_READY;
    while(res != FR_OK){
        res = f_close(&(pMsg->pFile->fileHandle));
        osi_Sleep(500);
    }
    UART_PRINT("%s closed with %d\n\r", pMsg->pFile->filename, res);
    return res;
#endif
}
