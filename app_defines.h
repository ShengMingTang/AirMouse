#ifndef APP_DEFINES_H_
#define APP_DEFINES_H_

//*****************************************************************************
//                 CUSTOM DEFINE -- Start
//*****************************************************************************
#define APPLICATION_NAME        "Air Mouse"
#define APPLICATION_VERSION     "1.4.0"
#define AP_SSID_LEN_MAX         (33)
#define ROLE_INVALID            (-5)

#define OOB_TASK_PRIORITY               (1)
#define OSI_STACK_SIZE                  (1600) // was 1024
#define SH_GPIO_3                       (3)  /* P58 - Device Mode */
#define ROLE_INVALID                    (-5)
#define AUTO_CONNECTION_TIMEOUT_COUNT   (50)   /* 5 Sec */


// /* Application specific status/error codes */
// typedef enum{
//     // Choosing this number to avoid overlap w/ host-driver's error codes
//     FILE_ALREADY_EXIST = -0x7D0,
//     FILE_CLOSE_ERROR = FILE_ALREADY_EXIST - 1,
//     FILE_NOT_MATCHED = FILE_CLOSE_ERROR - 1,
//     FILE_OPEN_READ_FAILED = FILE_NOT_MATCHED - 1,
//     FILE_OPEN_WRITE_FAILED = FILE_OPEN_READ_FAILED -1,
//     FILE_READ_FAILED = FILE_OPEN_WRITE_FAILED - 1,
//     FILE_WRITE_FAILED = FILE_READ_FAILED - 1,
    
//     STATUS_CODE_MAX = -0xBB8
// }e_AppStatusCodes;

//union
//{
//    char BsdBuf[BUF_SIZE];
//    unsigned long demobuf[BUF_SIZE/4];
//} uBuf;

// storage related
#define MAX_FILENAME_SIZE 20

#define TRANSFER_BLOCK_SIZE 50

#define PUSH_TOKEN_PREFIX "__SL_P_UZ"
#define PUSH_START_TOKEN PUSH_TOKEN_PREFIX "0"
#define PUSH_END_TOKEN PUSH_TOKEN_PREFIX "1"
#define PUSH_PROGRESS_TOKEN "__SL_P_U" // seq is not used now

#define PULL_TOKEN_PREFIX "__SL_P_Uz"
#define PULL_START_TOKEN PULL_TOKEN_PREFIX "0"
#define PULL_END_TOKEN PULL_TOKEN_PREFIX "1"

#define PULL_FETCH_PREFIX "__SL_G_UZ"
#define PULL_FILENAME_TOKEN PULL_FETCH_PREFIX "0"
#define PULL_READY_TOKEN PULL_TOKEN_PREFIX "1"
#define PULL_OFFSET_TOKEN PULL_TOKEN_PREFIX "2"

#define PULL_PROGRESS_TOKEN "__SL_G_U" // followed by seq

#define READY_TOKEN "Y"
#define NOT_READT_TOKEN "N"

#define FS_LIST_TOKEN_S "__SL_G_Ua0"
#define FS_LIST_TOKEN   "__SL_G_Ua"  // list from '/'
#define FS_STR_MAX_LEN TRANSFER_BLOCK_SIZE*26
// __SL_G_Uaa ~ __SL_G_Uaz is available

// #define PRINT_ROUTER

#if defined(PRINT_ROUTER)
#if defined(UART_PRINT)
#undef UART_PRINT
#define UART_PRINT printf
#endif
#endif

//*****************************************************************************
//                 MACRO FUNCTIONS
//*****************************************************************************
#define IS_TOKEN_MATCH(ptr, token) memcmp(ptr,token,strlen((const char *)token))==0
#define CHARS_PRINT(ptr, len)\
{\
    int i;\
    for(i = 0; i < (len); i++){\
        UART_PRINT("%c", ptr[i]);\
    }\
}
#define IS_EXPECTED_VALUE(value, expectedValue)\
{\
    if(value != expectedValue){\
        ERR_PRINT(value);\
        LOOP_FOREVER();\
    }\
}

#endif /* APP_DEFINES_H_ */
