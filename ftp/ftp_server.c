#include "ftp_server.h"
#include <ff.h>

static SemaphoreHandle_t semTaskRunning;
static FATFS fs;

void ftpServerTask(void *pvParameters)
{
    // socket related
    int	listenfd, connfd;
    int port = 21; // ftp port
    struct sockaddr_in	servaddr;
    // fatfs related
    FRESULT res = FR_NOT_READY;
    // local
    int retVal;

    if((semTaskRunning = xSemaphoreCreateCounting(MAX_NUM_TASKS, MAX_NUM_TASKS)) == NULL){
        printf("Create semTaskRunning Error\n\r");
        while(1);
    }

    // create a listening socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){ // error
        printf("listen Socket Error %d\r\n", listenfd);
        LOOP_FOREVER();
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons((unsigned short)port);
    if(retVal = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
        printf("bind Socket Error %d\r\n", retVal);
        while(1);
    }
    if(retVal = listen(listenfd, LISTENQ) < 0){
        printf("listen Socket Error %d\r\n", retVal);
        close(listenfd);
        while(1);
    }

    // fatfs init
    if((res = f_mount(&fs, "0", 1)) != FR_OK){ // immediately mounted
        printf("mount Error %d\n\r", res);
        while(1);
    }

    // wait for connectio and create separate children task to handle each client
    while(1){
        xSemaphoreTake(semTaskRunning, portMAX_DELAY); // limit max connection

        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        printf("New Client Detected ...\n\r");
        
        // check connfd is valid
        if(connfd < 0){ // error
            printf("accept Socket error %d, continue\n\r", connfd);
            xSemaphoreGive(semTaskRunning); // early exit
        }
        else{ // on receive a connection request
            int *param = (int*)malloc(sizeof(int)); // freed by children task itself
            if(!param){ // malloc error
                printf("malloc error\n\r");
                break;
            }
            else{ // create a stand alone task to handle that conn
                *param = connfd;
                if((retVal = xTaskCreate(ftpConnTask, "", CONN_STACK_SIZE, (void*)param, DATA_TASK_PRIOR, NULL)) != pdPASS){
                    printf("Create Conn task Error %d\n\r", retVal);
                    break;
                }
            }
            
        }
    }

    printf("Ftp server task dead\n\r");
    while(1);
}
void ftpConnTask(void *pvParameters)
{
    // socket related
    int connfd = *((int*)(pvParameters));
    int datafd;
    struct timeval tv;
    unsigned long cltIp, cltPort;
    unsigned long portScan = 5001; // port scan from 5001 down ward
    // parse command
    char recvline[MAXLINE+1];
    Cmd_t cmd = CMD_INVALID;
    char *parseHead = NULL;
    // user data
    char user[MAXUSERNAME], pass[MAXPASS], typeCode[MAXLINE];
    char cwd[MAXPATH];
    // local
    int retVal;
    int done = 0; // 0 for enter loop, 1 on break the loop
    int isConnOpen = 0;

    // init
    strcpy(cwd, "/");
    
    // protocol between task creator and task itself
    free(pvParameters);

    // configure socket
    tv.tv_sec = SOCK_TIMEOUT;
    tv.tv_usec = 0;
    if((retVal = setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
        printf("Set socket rcvtimeo Error %d\n\r", retVal);
        done = 1;
    }
    if((retVal = send(connfd, RESP_220_SRV_READY, strlen(RESP_220_SRV_READY), 0)) < 0){
        printf("Greeting send Error %d \n\r");
        while(1);
    }

    while(!done){
        // wait for data conn port
        printf("Receive:");
        if((retVal = recv(connfd, recvline, sizeof(recvline), 0)) < 0){ // timedout
            printf("Recv Error %d\n\r", retVal);
            if(retVal == EAGAIN){
                printf("Connection timeout leave\n\r");
                done = 1;
            }
            else{
                continue;
            }
        }
        recvline[retVal - 2] = '\0'; // close that line

        // parse command
        printf("\"%s\"\n\r", recvline);
        parseHead = ftpGetCmd(recvline, &cmd);
        printf("cmd: %d\n\r", cmd);

        switch (cmd)
        {
            case CMD_LIST:
                if(isConnOpen){
                    parseHead = ftpProcessList(parseHead, connfd, datafd);
                    isConnOpen = 0;
                }
                else{
                    printf("Conn not open\n\r");
                    if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                       printf("Send 503 Error %d\n\r", retVal); 
                       done = 1;
                    }
                }
                break;
            case CMD_RETR:
                if(isConnOpen){
                    parseHead = ftpProcessRetr(parseHead, connfd, datafd);
                    isConnOpen = 0;
                }
                else{
                    printf("Conn not open\n\r");
                    if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                       printf("Send 503 Error %d\n\r", retVal);
                       done = 1;
                    }
                }
                break;
            case CMD_STOR:
                if(isConnOpen){
                    parseHead = ftpProcessStor(parseHead, connfd, datafd);
                    isConnOpen = 0;
                }
                else{
                    printf("Conn not open\n\r");
                    if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                       printf("Send 503 Error %d\n\r", retVal);
                       done = 1;
                    }
                }
                break;
            case CMD_USER:
                ftpProcessUser(parseHead, connfd, user);
                break;
            case CMD_PASS:
                ftpProcessPass(parseHead, connfd, pass);
                break;
            case CMD_QUIT: // this task is done
                ftpProcessQuit(connfd);
                done = 1;
                printf("%lu:%lu quits\n\r", cltIp, cltPort);
                break;
            case CMD_TYPE:
                ftpProcessType(connfd, typeCode);
                break;
            case CMD_SYST:
                ftpProcessSyst(connfd);
                break;
            case CMD_PWD:
                ftpProcessPwd(connfd);
                break;
            case CMD_CWD:
                ftpProcessCwd(connfd, cwd);
                break;
            case CMD_PORT:
                ftpProcessPort(parseHead, connfd, &cltIp, &cltPort);
                if(ftpSetupDataConnActive(&datafd, cltIp, cltPort, portScan) < 0){
                    done = 1;
                }
                else{
                    isConnOpen = 1;
                }
                break;
            case CMD_PASV:
                ftpProcessPasv(parseHead, connfd, portScan, g_ulDeviceIp, &datafd);
                if(ftpSetupDataConnPassive(&datafd, cltIp, cltPort, portScan) < 0){
                    done = 1;
                }
                else{
                    isConnOpen = 1;
                }
                break;
            case CMD_NOOP: // send a simple response
                printf("Noop\n\r");
                ftpProcessNoop(connfd);
                break;
            case CMD_INVALID: // send not implemented response
                ftpProcessInvalid(connfd);
                break;
            default:
                break;
        }


    }

    close(connfd);
    close(datafd);
    xSemaphoreGive(semTaskRunning);
    vTaskDelete(NULL);
}
// OK
char* ftpGetCmd(char *str, Cmd_t *cmd)
{
    *cmd = CMD_INVALID;
    str = strtok(str, " \0");
    if(!strcmp(str, "LIST"))
        *cmd = CMD_LIST;
    else if(!strcmp(str, "RETR"))
        *cmd = CMD_RETR;
    else if(!strcmp(str, "STOR"))
        *cmd = CMD_STOR;
    else if(!strcmp(str, "USER"))
        *cmd = CMD_USER;
    else if(!strcmp(str, "PASS"))
        *cmd = CMD_PASS;
    else if(!strcmp(str, "QUIT"))
        *cmd = CMD_QUIT;
    else if(!strcmp(str, "TYPE"))
        *cmd = CMD_TYPE;
    else if(!strcmp(str, "SYST"))
        *cmd = CMD_SYST;
    else if(!strcmp(str, "PWD"))
        *cmd = CMD_PWD;
    else if(!strcmp(str, "CWD"))
        *cmd = CMD_CWD;
    else if(!strcmp(str, "PORT"))
        *cmd = CMD_PORT;
    else if(!strcmp(str, "PASV"))
        *cmd = CMD_PASV;
    else if(!strcmp(str, "NOOP"))
        *cmd = CMD_NOOP;
    
    return str; // for next parser
}

char* ftpProcessList(char *str, int connfd, int datafd)
{
    char dir[MAXBUFF];
    char *p;
    if(str == NULL){
        dir[0] = '\0';
    }
    else{
        p = strtok(NULL, "\0");
        strcpy(dir, p);
    }

    printf("Listing \"%s\"\n\r", dir);

    // check dir status @@bypass and @@testing
    // send(connfd, RESP_125_TRANS_START, strlen(RESP_125_TRANS_START), 0);
    // send(datafd, "-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n", strlen("-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n"), 0);
    // if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
    //     printf("Send 226 Error %d\n\r", retVal);
    // }

    ftpStorageList(connfd, datafd, dir);

    close(datafd);
    return NULL;
}
char* ftpProcessRetr(char *str, int connfd, int datafd)
{
    char *path = strtok(NULL, "\0");

    printf("Retrieving \"%s\"\n\r", path);

    // check file status @@bypass and @@testing
    // int i, retVal;
    // static char bulkTest[1024];
    // send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0);
    // for(i = 0; i < 1024; i++){
    //     bulkTest[i] = '0' + i % 10;
    // }
    // bulkTest[1022] = '\r';
    // bulkTest[1023] = '\n';
    // printf("Sending...\n\r");
    // for(i = 0; i < 60*1024; i++){
    //     if((retVal = send(datafd, bulkTest, sizeof(bulkTest), 0)) < 0){
    //         printf("Error %d in sending\n\r", retVal);
    //         i--;
            // vTaskDelay(pdMS_TO_TICKS(SOCK_BREAK_MS));
    //     }
    // }
    // printf("Finish sending...\n\r");
    // close(datafd);

    ftpStorageRetr(connfd, datafd, path);
    close(datafd);


    return NULL;
}
char* ftpProcessStor(char *str, int connfd, int datafd)
{
    int i;
    int retVal = 0;
    char *path = strtok(NULL, "\r\n");
    char buff[MAXBUFF];

    printf("Storing %s\n\r", path);

    // check file status @@bypass
    send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0);
    
    // transmit @@testing
    while((retVal = recv(datafd, buff, sizeof(buff), 0)) > 0){
        printf("Recv(%d):", retVal);
        for(i = 0; i < 5; i++){// list first 5 only
            printf("%02X", buff[i]);
        }
        if(retVal > 5) printf("...");
        printf("\n\r");
    }

    printf("At the end of stor Error code is %s\n\r", retVal);
    // close
    send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0);
    close(datafd);

    return NULL;
}

void ftpProcessPort(char *str, int connfd, unsigned long *cltIp, unsigned long *cltPort)
{
    int retVal;
    char *n1, *n2, *n3, *n4, *n5, *n6;

    // str = "ip[3],ip[2],ip[1],ip[0],port[1],port[0]"
	n1 = strtok(NULL, ",");
	n2 = strtok(NULL, ",");
	n3 = strtok(NULL, ",");
	n4 = strtok(NULL, ",");
	n5 = strtok(NULL, ",");
	n6 = strtok(NULL, ",");

    *cltIp = (atoi(n1) << 24) | (atoi(n2) << 16) | (atoi(n3) << 8) | (atoi(n4));
    *cltPort = (atoi(n5) << 8) | (atoi(n6));

    printf("Client Ip: %d.%d.%d.%d\n\r", 
        *cltIp & 0xFF000000,
        *cltIp & 0x00FF0000,
        *cltIp & 0x0000FF00,
        *cltIp & 0x000000FF
    );
    printf("Client Port: %d\n\r", *cltPort);

    retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0);
    if(retVal < 0){
        printf("send PORT resp Error %d\n\r", retVal);
    }
}
int ftpProcessPasv(char *str, int connfd, unsigned short portScan, unsigned long srvIp, int *datafd)
{
    int retVal;
    struct sockaddr_in tempaddr;
    int transfd;
    struct timeval tv;
    char resp[64];
    // int nonblocking = 1;

	if ( (*datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Open datafd error %d", *datafd);
    	return -1;
    }
    tv.tv_sec = SOCK_TIMEOUT;
    tv.tv_usec = 0;
    if((retVal = setsockopt(*datafd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
        printf("Set socket rcvtimeo Error %d\n\r", retVal);
        return -1;
    }

	//bind port for data connection to be server port - 1 by using a temporary struct sockaddr_in
    memset(&tempaddr, 0, sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    tempaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    tempaddr.sin_port   = htons(portScan); // was portScan - 1

    // find one port the server can use
    while(portScan > 0 && (bind(*datafd, (struct sockaddr*) &tempaddr, sizeof(tempaddr))) < 0){
        portScan--;
    	tempaddr.sin_port = htons(portScan);
    }
    if(portScan == 0){
        printf("Cannot found a server port to use\n\r");
        return -1;
    }

    printf("use port %d to accept connection\n\r", portScan);
    if((retVal = listen(*datafd, LISTENQ)) < 0){ // fail
        printf("listen data socket Error %d\n\r", retVal);
        close(*datafd);
        if((retVal = send(connfd, RESP_421_SRV_NOT_AVAI, strlen(RESP_421_SRV_NOT_AVAI), 0)) < 0){
            printf("send 421 Error %d\n\r", retVal);
        }
        return -1;
    }
    else{ // success
        sprintf(resp, "227 Entering Passive Mode (%lu,%lu,%lu,%lu,%hu,%hu).\r\n", 
            (unsigned long)IPV4_BYTE(srvIp, 3), (unsigned long)IPV4_BYTE(srvIp, 2), (unsigned long)IPV4_BYTE(srvIp, 1), (unsigned long)IPV4_BYTE(srvIp, 0),
            (unsigned short)IPV4_BYTE(portScan, 1), (unsigned short)IPV4_BYTE(portScan, 0)
        );

        if(retVal = send(connfd, resp, strlen(resp), 0) < 0){
            printf("send 227 Error %d\n\r", retVal);
            return -1;
        }
        else{ // accepting connection
            printf("resp: %s\n\r", resp);
            printf("accepting trasn conn\n\r");

            // if((retVal = setsockopt(*datafd, , SOL_SOCKET, SO_NONBLOCKING, (const char*)&nonblocking, sizeof(nonblocking))) < 0){
            //     printf("set to nonblocking Error %d\n\r", retVal);
            //     close(*datafd);
            //     return -1;
            // }

            if((transfd = accept(*datafd, (struct sockaddr*)NULL, NULL)) < 0){
                printf("transfd Error %d\n\r", transfd);
                return -1;
            }
            printf("accepted\n\r");

            close(*datafd);
            *datafd = transfd;
        }
    }

    return 0;
}

// @@ all pass for now
void ftpProcessUser(char *str, int connfd, char *user)
{
    int retVal;
    char *p = strtok(NULL, "\r");
    strncpy(user, p, MAXUSERNAME);
    printf("Username: %s\n\r", p);
    // always pass now
    if((retVal = send(connfd, RESP_331_SPEC_PASS, strlen(RESP_331_SPEC_PASS), 0)) < 0){
        printf("331 reply errror\n\r");
    }
}
// @@ not verified for now
void ftpProcessPass(char *str, int connfd, char *pass)
{
    int retVal;
    char *p = strtok(NULL, "\r");
    strncpy(pass, p, MAXPASS);
    printf("Password: %s\n\r", p);
    // always pass now
    if((retVal = send(connfd, RESP_230_LOGIN_SUCC, strlen(RESP_230_LOGIN_SUCC), 0)) < 0){
        printf("230 reply errror\n\r");
    }
}
// OK
void ftpProcessQuit(int connfd)
{
    int retVal;
    retVal = send(connfd, RESP_221_GOODBYE, strlen(RESP_221_GOODBYE), 0);
    if(retVal < 0){
        printf("send good bye msg Error %d, close anyway\n\r", retVal);
    }
}
// @@ type code does not change any behavior now
void ftpProcessType(int connfd, char *typeCode)
{
    int retVal;
    char *p = strtok(NULL, "\r");
    strncpy(typeCode, p, MAXPASS);
    printf("Type code: %s\n\r", p);
    // always pass now
    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("200 reply errror %d\n\r", retVal);
    }
}
// OK
void ftpProcessSyst(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_215_SYS_TYPE, strlen(RESP_215_SYS_TYPE), 0)) < 0){
        printf("215 reply errror %d\n\r", retVal);
    }
}
void ftpProcessPwd(int connfd)
{
    int retVal;
    char resp[MAXBUFF];
    char dir[MAXBUFF];
    
    // always pass now
    f_getcwd(dir, sizeof(dir));

    sprintf(resp, "257 \"%s\"created\r\n", dir);
    if((retVal = send(connfd, resp, strlen(resp), 0)) < 0){
        printf("257 reply errror %d\r\n", retVal);
    }

    printf("pwd: %s\n\r", dir);
}
void ftpProcessCwd(int connfd, char *path)
{
    FRESULT res;
    int retVal;
    char *p = strtok(NULL, "\r");
    char buff[MAXBUFF];
    
    // always pass now
    if((res = f_chdir(p)) != FR_OK){
        printf("chdir Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("550 reply errror %d\r\n", retVal);
        }
    }
    else{
        if((retVal = send(connfd, RESP_250_FILE_ACT_OK, strlen(RESP_250_FILE_ACT_OK), 0)) < 0){
            printf("250 reply errror %d\r\n", retVal);
        }
        f_getcwd(buff, sizeof(buff));
        printf("cwd to %s", buff);
    }
    
}
// OK
void ftpProcessNoop(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("send 200 Error %d\n\r", retVal);
    }
}
// OK
void ftpProcessInvalid(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
        printf("send 503 Error %d\n\r", retVal);
    }
}

// not used
int  ftpSetupDataConnActive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan)
{
    int retVal;
    struct sockaddr_in cliaddr, tempaddr;
    struct timeval tv;

	if ( (*datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Open datafd error");
    	return -1;
    }
    tv.tv_sec = SOCK_TIMEOUT;
    tv.tv_usec = 0;
    if((retVal = setsockopt(*datafd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
        printf("Set socket rcvtimeo Error %d\n\r", retVal);
        return -1;
    }

	//bind port for data connection to be server port - 1 by using a temporary struct sockaddr_in
    memset(&tempaddr, 0, sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    tempaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    tempaddr.sin_port   = htons(portScan); // was portScan - 1

    // find one port the server can use
    while(portScan > 0 && (bind(*datafd, (struct sockaddr*) &tempaddr, sizeof(tempaddr))) < 0){
        portScan--;
    	tempaddr.sin_port = htons(portScan);
    }
    if(portScan == 0){
        printf("Cannot found a server port to use\n\r");
        return -1;
    }
    printf("use port %d to connect\n\r", portScan);

	//initiate data connection fd with client ip and client port             
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port   = htons(cltPort);
    cliaddr.sin_addr.s_addr = htonl((unsigned long)cltIp);

    if (connect(*datafd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0){
        printf("Connect to client@%04X:%02X Failed\n\r", cliaddr.sin_addr.s_addr, cliaddr.sin_port);
    	return -1;
    }

    printf("Connected to client@%04X:%02X successfully\n\r", cliaddr.sin_addr.s_addr, cliaddr.sin_port);

    return 0;
}
int  ftpSetupDataConnPassive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan)
{
    return 1;
}
