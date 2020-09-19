#ifndef FTP_SERVER_H
#define FTP_SERVER_H

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// FreeRTOS includes
#include <FreeRTOS.h>
#include <projdefs.h>
#include <task.h>
#include <semphr.h>
#include <event_groups.h>
// fatfs includes
#include <ff.h>
// board dependent includes
#include "ftp_board.h" // provide interface between bsd APIs and simplink APIs

/* source code build route */
#define VERBOSE
// #define TEST
// #define PIPEPLINED // defined then use pipeline scheme
// #define SUPPORT_ACTIVE_CONN

// warnings
#if defined(VERBOSE)
#warning "Verbose mode is used"
#endif
#if defined(TEST)
#warning "FTP server is running at test mode"
#endif
#if defined(PIPEPLINED)
#warning "Pipeline scheme is used for storage, now only RETR is affected. NOT ALL TEST PASSED"
#endif
#if defined(SUPPORT_ACTIVE_CONN)
#warning "Active Connection is supported"
#else
#warning "Active Connection is NOT supported"
#endif


// IP layer varabiles
extern unsigned long  g_ulDeviceIp;
// helpers
#define IPV4_BYTE(val,index) ( (val >> (index*8)) & 0xFF )

// sizes
#define MAX_NUM_TASKS (1)
#define MAXLINE (64)
#define MAXCMD  (64)
#define MAXUSERNAME (32)
#define MAXPASS (32)
#define MAXPATH (64)
#define MAXBUFF (128)
#define MAXTRANS (1024)
#define	LISTENQ	(1024)
// stack sizes, related to the above sizes
#define SERVER_STACK_SIZE (1024)
#define CONN_STACK_SIZE (3072)
#define STR_STCK_SIZE (512)
#define DATA_TASK_PRIOR (8)
#define STR_TASK_PRIOR (8)
// socket options
#define FTP_PORT (21)
#define CONN_SOCK_TIMEOUT (10)
#define DATA_SOCK_TIMEOUT (3)
#define INV_CMD_TIMES (5) // max number of invalud command a client can send
#define SOCK_BREAK_MS (50)
#define PASV_TRY_TIMES (3)
// timeout for event wait
#define FILSTR_EVENT_WAIT_MS (2000)

// messages
#define RESP_125_TRANS_START "125 Data connection already open; transfer starting.\r\n"
#define RESP_150_FILE_STAT_OK "150 File status okay; about to open data connection.\r\n"
#define RESP_200_CMD_OK "200 Command okay.\r\n"
#define RESP_202_CMD_NOT_IMPL "202 Command not implemented, superfluous at this site.\r\n"
#define RESP_215_SYS_TYPE "215 UNIX Type: L8\r\n"
#define RESP_220_SRV_READY "220 Service ready for new user\r\n"
#define RESP_221_GOODBYE "221 Goodbye\r\n"
#define RESP_226_TRANS_COMPLETE " 226 Closing data connection.\r\n"
#define RESP_230_LOGIN_SUCC "230 User logged in successfully.\r\n"
#define RESP_250_FILE_ACT_OK "250 Requested file action okay, completed\r\n"
#define RESP_331_SPEC_PASS "331 User name okay, need password.\r\n"
#define RESP_350_FILE_ACT_PEND "350 Requested file action pending further information.\r\n"
#define RESP_421_SRV_NOT_AVAI "421 Service not available, closing control connection.\r\n"
#define RESP_451_REQ_ABR "451 Requested action aborted: local error in processing.\r\n"
#define RESP_503_BAD_SEQ "503 Bad sequence of commands.\r\n"
#define RESP_530_NOT_LOG_IN "530 Not logged in.\r\n"
#define RESP_550_REQ_NOT_TAKEN "550 Requested action not taken.\r\n"
#define RESP_553_REQ_NOT_TAKEN_FN_NOT_ALLOWED "553 Requested action not taken.File name not allowed.\r\n"

// cmds
typedef enum{
/* user */
    CMD_USER,
    CMD_PASS,
    CMD_QUIT,
    CMD_PASV,
#if defined(SUPPORT_ACTIVE_CONN)
    CMD_PORT,
#endif
/* file operation */
    CMD_RETR, // data
    CMD_STOR, // data
    CMD_DELE, // delete a file or dir
    CMD_RNFR,
    CMD_RNTO,
/* directory */
    CMD_LIST, // data
    CMD_PWD,
    CMD_CWD,
    CMD_MKD,
    CMD_RMD,
/* common */
    CMD_TYPE,
    CMD_SYST,
    CMD_NOOP,
    CMD_INVALID
}Cmd_t;

#if defined(PIPELINED)
// events between streaming task and conn task
#define FILSTR_SOCK_DONE (0x01)
#define FILSTR_DISK_DONE (0x02)
#define FILSTR_EOF (0x04)
// param for streaming task
typedef struct{
    // common
    FIL *fp;
    EventGroupHandle_t event;
    // callback for disk r/w
    FRESULT (*f)(FIL* fp, void* buff, UINT btw, UINT* bw);
    // buffer and transmitted len
    char *buff;
    UINT btf; // byte transfered of last copy
}FileStream_t;
#endif

#define INPUT
#define OUTPUT

void ftpServerInit();

// listening at host:21 and create ftpConnTask
// to handle each client separately
void ftpServerTask(void *pvParameters);

// handle to a client, each has a ftpDataTask to handle transmission
// control channel handling  
void ftpConnTask(void *pvParameters);

// parse command
int ftpGetCmd(char *str, OUTPUT Cmd_t *cmd);

/* user */
int ftpProcessUser(int connfd, char *str, OUTPUT char *user);
int ftpProcessPass(int connfd, char *user, char *pass);
int ftpProcessQuit(int connfd);
int ftpProcessPasv(int connfd, unsigned short portScan, unsigned long srvIp, OUTPUT int *datafd);
/* file operation */
int ftpProcessRetr(int connfd, int datafd, char *str);
int ftpProcessStor(int connfd, int datafd, char *str);
int ftpProcessDele(int connfd, int datafd, char *str);
int ftpProcessRnfr(int connfd, int datafd, char *str, char *fr);
int ftpProcessRnto(int connfd, int datafd, char *str, char *fr);
/* directory */
int ftpProcessList(int connfd, int datafd, char *str);
int ftpProcessPwd(int connfd);
int ftpProcessCwd(int connfd, char *arg);
int ftpProcessMkd(int connfd, int datafd, char *str);
int ftpProcessRmd(int connfd, int datafd, char *str);
/* common */
int ftpProcessSyst(int connfd);
int ftpProcessType(int connfd, char *arg, OUTPUT char *typeCode);
int ftpProcessNoop(int connfd);
int ftpProcessInvalid(int connfd);


// lower order functions for interfacing with fs
#if defined(PIPELINED)
void ftpStreamTask(void *pvParameters);
#endif
int ftpStorageInit();
// assume fs is mounted before all these functions are called
int verifyUserPass(char *user, char *pass);

int ftpStorageRetr(int connfd, int datafd, char *path);
int ftpStorageStor(int connfd, int datafd, char *path);
int ftpStorageDele(int connfd, int datafd, char *path);
int ftpStorageRnfr(int connfd, int datafd, char *path);
int ftpStorageRnto(int connfd, int datafd, char *path, char *npath);

int ftpStorageList(int connfd, int datafd, char *path);
int ftpStoragePwd(int connfd);
int ftpStorageCwd(int connfd, char *path);
int ftpStorageMkd(int connfd, int datafd, char *path);
int ftpStorageRmd(int connfd, int datafd, char *path);


#if defined(SUPPORT_ACTIVE_CONN)
int ftpProcessPort(int connfd, OUTPUT unsigned long *cltIp, OUTPUT unsigned long *cltPort);
int  ftpSetupDataConnActive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan);
#endif

#endif
