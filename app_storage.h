#ifndef APP_STORAGE_H_
#define APP_STORAGE_H_

#include "app_defines.h"

#define OPEN_FILE_SIZE 4096

#define IS_FILE_HANDLE_VALID(handle) (handle)>=0

#define USE_SERIAL_FLASH

typedef enum{
    STORAGE_OP_OPEN_WRITE,
    STORAGE_OP_OPEN_READ,
    STORAGE_OP_WRITE,
    STORAGE_OP_READ,
    STORAGE_OP_CLOSE,
    STORAGE_OP_INVALID
}StorageOp_e;

typedef struct
{
    char filename[20];
    unsigned long token;
    long offset; // should be initialized
    long fileHandle;
}StorageFile_t;

typedef struct
{
    char msg[256]; // set by signal
    long msgLen; // set by signal
    StorageOp_e op; // set by signal
    OsiLockObj_t *pLock;
    OsiSyncObj_t *pSync;
    StorageFile_t *pFile; // manually assign
}StorageMsg_t;

void storageControllerTask(void *pvParameters);

long appCreateFile(StorageMsg_t *pMsg);
long appOpenFile(StorageMsg_t *pMsg, long mode);
long appWriteToFile(StorageMsg_t *pMsg);
long appReadFromeFile(StorageMsg_t *pMsg);
long appCloseFile(StorageMsg_t *pMsg);

#endif /* APP_STORAGE_H_ */
