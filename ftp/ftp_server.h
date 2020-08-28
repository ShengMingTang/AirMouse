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

// IP layer varabiles
extern unsigned long  g_ulDeviceIp;

#define IPV4_BYTE(val,index) ( (val >> (index*8)) & 0xFF )

#define MAX_NUM_TASKS 2
#define MAXLINE 64
#define MAXCMD  64
#define MAXUSERNAME 32
#define MAXPASS 32
#define MAXPATH 64
#define MAXBUFF 128
#define MAXTRANS 1024 // was 1024
#define	LISTENQ	1024
#define SERVER_STACK_SIZE 1024
#define CONN_STACK_SIZE 1024
#define STR_STCK_SIZE 3072 // was 3096
#define DATA_TASK_PRIOR 1
#define STR_TASK_PRIOR 1

// cmds
typedef enum{
    CMD_LIST,
    CMD_RETR,
    CMD_STOR,
    CMD_PORT,
    CMD_PASV,
    CMD_USER,
    CMD_PASS,
    CMD_QUIT,
    CMD_SYST,
    CMD_TYPE,
    CMD_PWD,
    CMD_CWD,
    CMD_NOOP,
    CMD_INVALID
}Cmd_t;

// events between streaming task and conn task
#define FILSTR_SOCK_DONE 0x01
#define FILSTR_DISK_DONE 0x02
#define FILSTR_EOF 0x04
// timeout for event wait
#define FILSTR_EVENT_WAIT_MS 10000
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

// socket option
#define SOCK_TIMEOUT 30
#define SOCK_BREAK_MS 50
#define IDLE_TIMEOUT 3 // wait max IDLE_TIMEOUT * SOCK_TIMEOUT seconds

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
#define RESP_421_SRV_NOT_AVAI "421 Service not available, closing control connection.\r\n"
#define RESP_503_BAD_SEQ "503 Bad sequence of commands.\r\n"
#define RESP_550_REQ_NOT_TAKEN "550 Requested action not taken.\r\n"

// event group defines
// #define EVT_MS_TO_WAIT 5000
// #define LIST_EVT 0x01
// #define RETR_EVT 0x02
// #define STOR_EVT 0x04
// #define USER_EVT 0x08
// #define ACCEPT_EVT 0x0A
// #define TIMEOUT_EVT 0x0C

#define INPUT
#define OUTPUT

// listening at host:21 and create ftpConnTask
// to handle each client separately
void ftpServerTask(void *pvParameters);

// handle to a client, each has a ftpDataTask to handle transmission
// control channel handling  
void ftpConnTask(void *pvParameters);

// parse command
char* ftpGetCmd(char *str, OUTPUT Cmd_t *cmd);

char* ftpProcessList(char *str, int connfd, int datafd);
char* ftpProcessRetr(char *str, int connfd, int datafd);
char* ftpProcessStor(char *str, int connfd, int datafd);
void ftpProcessPort(char *str, int connfd, OUTPUT unsigned long *cltIp, OUTPUT unsigned long *cltPort);

// create datafd ready for accept connection
int ftpProcessPasv(char *str, int connfd, unsigned short portScan, unsigned long srvIp, OUTPUT int *datafd);
void ftpProcessUser(char *str, int connfd, OUTPUT char *user);
void ftpProcessPass(char *str, int connfd, OUTPUT char *pass);
void ftpProcessQuit(int connfd);
void ftpProcessSyst(int connfd);
void ftpProcessType(int connfd, OUTPUT char *typeCode);
void ftpProcessPwd(int connfd);
void ftpProcessCwd(int connfd, char *path);
void ftpProcessNoop(int connfd);
void ftpProcessInvalid(int connfd);

int  ftpSetupDataConnActive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan);

int  ftpSetupDataConnPassive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan);

// lower order functions for interfacing with fs
void ftpStreamTask(void *pvParameters);
int ftpStorageList(int connfd, int datafd, char *path);
int ftpStorageRetr(int connfd, int datafd, char *filename);

#endif
