#ifndef APP_STORAGE_H_
#define APP_STORAGE_H_
#include "app_defines.h"
#include "ff.h"

#define OPEN_FILE_SIZE 4096

#define IS_FILE_HANDLE_VALID(handle) (handle)>=0


// #storage
// #define USE_BUFFER
// #define USE_SL_FS
#define FATFS_MOUNT_OPT 1 // immediately mounted


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
#if defined(USE_BUFFER)
#elif defined(USE_SL_FS)
    long fileHandle;
#else
    FIL fileHandle;
#endif
}StorageFile_t;

typedef struct
{
    char msg[256]; // set by signal
    unsigned int msgLen; // set by signal
    StorageOp_e op; // set by signal
    OsiLockObj_t *pLock; // lock before any file operation
    OsiSyncObj_t *pSync; // signal by http server task
    StorageFile_t *pFile; // manually assign
}StorageMsg_t;

long storageFatFsInit();

void storageControllerTask(void *pvParameters);
void listDirectory(DIR *dir);
long appCreateFile(StorageMsg_t *pMsg);
long appOpenFile(StorageMsg_t *pMsg, long mode);
long appWriteToFile(StorageMsg_t *pMsg);
long appReadFromeFile(StorageMsg_t *pMsg);
long appCloseFile(StorageMsg_t *pMsg);

#endif /* APP_STORAGE_H_ */
