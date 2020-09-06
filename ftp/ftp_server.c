#include "ftp_server.h"

static SemaphoreHandle_t semTaskRunning;

void ftpServerTask(void *pvParameters)
{
    // socket
    int	listenfd, connfd;
    struct sockaddr_in	servaddr;
    // local
    int retVal;
    int param = 0; // parameter for child connTask

    if(ftpStorageInit() < 0){
        vTaskDelete(NULL);
    }
    if((semTaskRunning = xSemaphoreCreateCounting(MAX_NUM_TASKS, MAX_NUM_TASKS)) == NULL){
        printf("Create semTaskRunning Error\n\r");
        vTaskDelete(NULL);
    }

    // create a listening socket
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ // error
        printf("listen Socket Error %d\r\n", listenfd);
        vTaskDelete(NULL);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons((unsigned short)FTP_PORT);
    if(retVal = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
        printf("bind Socket Error %d\r\n", retVal);
        close(listenfd);
        vTaskDelete(NULL);
    }
    if(retVal = listen(listenfd, LISTENQ) < 0){
        printf("listen Socket Error %d\r\n", retVal);
        close(listenfd);
        vTaskDelete(NULL);
    }

    // wait for connectio and create separate children task to handle each client
    while(1){
        xSemaphoreTake(semTaskRunning, portMAX_DELAY); // limit max connection

        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        printf("New Client Detected ...\n\r");
        
        // check connfd is valid
        if(connfd < 0){
            printf("accept Socket error %d, continue listening\n\r", connfd);
            xSemaphoreGive(semTaskRunning);
        }
        else{ // on receiving a connection request
            while(param){} // loop until child task set this to 0

            param = connfd; // connfd is closed by its children
            if((retVal = xTaskCreate(ftpConnTask, "", CONN_STACK_SIZE, (void*)(&param), DATA_TASK_PRIOR, NULL)) != pdPASS){
                printf("Create Conn task Error %d\n\r", retVal);
                break;
            }
            
        }
    }

    printf("Ftp server task dead\n\r");
    close(listenfd);
    vTaskDelete(NULL);
}
void ftpConnTask(void *pvParameters)
{
    // socket
    int connfd = *((int*)(pvParameters));
    int datafd = -1;
    struct timeval tv;
    unsigned long cltIp, cltPort;
    unsigned long portScan = 5001; // port scan from 5001 down ward
    // command
    char recvline[MAXLINE];
    char *scmd, *arg;
    Cmd_t cmd = CMD_INVALID;
    char *parseHead = NULL;
    // user data
    char user[MAXUSERNAME], pass[MAXPASS];
    char typeCode[MAXLINE];
    char rn[MAXPATH];
    // local
    int retVal;
    int done = 0; // 0 for enter loop, 1 on break the loop
    int isConnOpen = 0;
    
    // protocol between task creator and task itself
    *((int*)pvParameters) = 0;

    // configure socket, won't enter loop if error
    tv.tv_sec = CONN_SOCK_TIMEOUT;
    tv.tv_usec = 0;
    if((retVal = setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
        printf("Set socket rcvtimeo Error %d\n\r", retVal);
        done = 1;
    }
    if((retVal = send(connfd, RESP_220_SRV_READY, strlen(RESP_220_SRV_READY), 0)) < 0){
        printf("Greeting send Error %d \n\r");
        done = 1;
    }

    while(!done){
        // wait for data conn port
        if((retVal = recv(connfd, recvline, sizeof(recvline), 0)) < 0){ // timedout
            if(retVal == EAGAIN){
                printf("Connection timeout leave\n\r");
                done = 1;
            }
            else{ // unrecoverable
                printf("<Error %d>\n\r", retVal);
                close(connfd);
                break;
            }
        }
        recvline[retVal - 2] = '\0'; // close that line
#if defined(VERBOSE)
        printf("Received : \"%s\"\n\r", recvline);
#endif

        scmd = strtok(recvline, " \0"); // strtok across scopes
        ftpGetCmd(scmd, &cmd);
        arg = strtok(NULL, "\0");
#if defined(VERBOSE)
        printf("cmd: %d\n\r", cmd);
#endif
        switch (cmd)
        {
        /* user */
            case CMD_USER:
                if(ftpProcessUser(connfd, arg, user)){
                    done = 1;
                }
                break;
            case CMD_PASS:
                if(ftpProcessPass(connfd, user, arg)){
                    done = 1;
                }
                break;
            case CMD_QUIT: // exit anyway
                ftpProcessQuit(connfd);
                done = 1;
#if defined(VERBOSE)
                printf("%s quits\n\r", user);
#endif
                break;
        /* file operation */
            case CMD_RETR:
                if(isConnOpen){
                    ftpProcessRetr(connfd, datafd, arg);
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
                    ftpProcessStor(connfd, datafd, arg);
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
            case CMD_DELE:
                ftpProcessDele(connfd, datafd, arg);
                break;
            case CMD_RNFR:
                ftpProcessRnfr(connfd, datafd, arg, rn);
                break;
            case CMD_RNTO:
                ftpProcessRnto(connfd, datafd, arg, rn);
                break;
        /* direcotry */
            case CMD_LIST:
                if(isConnOpen){
                    ftpProcessList(connfd, datafd, arg);
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
            case CMD_PWD:
                if(ftpProcessPwd(connfd)){
                    done = 1;
                }
                break;
            case CMD_CWD:
                if(ftpProcessCwd(connfd, arg)){
                    done = 1;
                }
                break;
            case CMD_MKD:
                ftpProcessMkd(connfd, datafd, arg);
                break;
            case CMD_RMD:
                ftpProcessRmd(connfd, datafd, arg);
                break;
            /* common */
            case CMD_TYPE:
                ftpProcessType(connfd, arg, typeCode);
                break;
            case CMD_SYST:
                if(ftpProcessSyst(connfd) < 0){
                    done = 1;
                }
                break;
//            case CMD_PORT:
//                ftpProcessPort(parseHead, connfd, &cltIp, &cltPort);
//                if(ftpSetupDataConnActive(&datafd, cltIp, cltPort, portScan) < 0){
//                    done = 1;
//                }
//                else{
//                    isConnOpen = 1;
//                }
//                break;
            case CMD_PASV:
                ftpProcessPasv(connfd, portScan, g_ulDeviceIp, &datafd);
                isConnOpen = 1;
                break;
            case CMD_NOOP:
                if(ftpProcessNoop(connfd) < 0){
                    done = 1;
                }
                break;
            case CMD_INVALID:
                if(ftpProcessInvalid(connfd) < 0){
                    done = 1;
                }
                break;
            default:
                break;
        }


    }

    close(connfd);
    if(datafd >= 0) close(datafd);
    xSemaphoreGive(semTaskRunning);
    vTaskDelete(NULL);
}
// OK
int ftpGetCmd(char *str, Cmd_t *cmd)
{
    // @@ can be made case insensitive comparison
    *cmd = CMD_INVALID;
    if(!strcmp(str, "LIST"))
        *cmd = CMD_LIST;
    else if(!strcmp(str, "RETR"))
        *cmd = CMD_RETR;
    else if(!strcmp(str, "STOR"))
        *cmd = CMD_STOR;
    else if(!strcmp(str, "DELE"))
        *cmd = CMD_DELE;
    else if(!strcmp(str, "RNFR"))
        *cmd = CMD_RNFR;
    else if(!strcmp(str, "RNTO"))
        *cmd = CMD_RNTO;
    else if(!strcmp(str, "MKD"))
        *cmd = CMD_MKD;
    else if(!strcmp(str, "RMD"))
        *cmd = CMD_RMD;
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
    // else if(!strcmp(str, "PORT"))
    //     *cmd = CMD_PORT;
    else if(!strcmp(str, "PASV"))
        *cmd = CMD_PASV;
    else if(!strcmp(str, "NOOP"))
        *cmd = CMD_NOOP;
    
    return 0;
}
int ftpProcessUser(int connfd, char *str, char *user)
{
    int retVal;

    strncpy(user, str, MAXUSERNAME);

#if defined(VERBOSE)
    printf("User: %s\n\r", user);
#endif
    // always pass now
    if((retVal = send(connfd, RESP_331_SPEC_PASS, strlen(RESP_331_SPEC_PASS), 0)) < 0){
        printf("331 reply Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
int ftpProcessPass(int connfd, char *user, char *pass)
{
    int retVal;

#if defined(VERBOSE)
    printf("Password: %s\n\r", pass);
#endif

    if(verifyUserPass(user, pass)){
        if((retVal = send(connfd, RESP_530_NOT_LOG_IN, strlen(RESP_530_NOT_LOG_IN), 0)) < 0){
            printf("530 reply Error %d\n\r", retVal);
            return -1;
        }    
    }
    else{
        if((retVal = send(connfd, RESP_230_LOGIN_SUCC, strlen(RESP_230_LOGIN_SUCC), 0)) < 0){
            printf("230 reply errror\n\r");
            return -1;
        }
    }

    return 0;
}
int ftpProcessQuit(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_221_GOODBYE, strlen(RESP_221_GOODBYE), 0)) < 0){
        printf("send good bye msg Error %d\n\r", retVal);
        return -1;
    }
    return 0;
}

int ftpProcessRetr(int connfd, int datafd, char *path)
{
#if defined(VERBOSE)
    printf("Retrieving \"%s\"\n\r", path);
#endif

#if defined(TEST)
    int i, retVal;
    static char bulkTest[1024];
    send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0);
    for(i = 0; i < 1024; i++){
        bulkTest[i] = '0' + i % 10;
    }
    bulkTest[1022] = '\r';
    bulkTest[1023] = '\n';
    printf("Sending...\n\r");
    for(i = 0; i < 60*1024; i++){
        if((retVal = send(datafd, bulkTest, sizeof(bulkTest), 0)) < 0){
            printf("Error %d in sending\n\r", retVal);
            i--;
            vTaskDelay(pdMS_TO_TICKS(SOCK_BREAK_MS));
        }
    }
    printf("Finish sending...\n\r");
    close(datafd);
#elif defined(SD_CARD_PRESENT)
    ftpStorageRetr(connfd, datafd, path);
#endif
    close(datafd);
    return 0;
}
int ftpProcessStor(int connfd, int datafd, char *path)
{
#if defined(VERBOSE)
    printf("Storing %s\n\r", path);
#endif

#if defined(TEST)
    // check file status @@bypass and @@testing
    int i;
    int retVal = 0;
    char buff[MAXBUFF];
    send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0);
    while((retVal = recv(datafd, buff, sizeof(buff), 0)) > 0){
        printf("Recv(%d):", retVal);
        for(i = 0; i < 5; i++){// list first 5 only
            printf("%02X", buff[i]);
        }
        if(retVal > 5) printf("...");
        printf("\n\r");
    }
    send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0);
#else
    ftpStorageStor(connfd, datafd, path);
#endif
    close(datafd);
    return 0;
}
int ftpProcessDele(int connfd, int datafd, char *path)
{
#if defined(VERBOSE)
    printf("Deleting %s\n\r", path);
#endif
    ftpStorageDele(connfd, datafd, path);
    close(datafd);
    return 0;
}
int ftpProcessRnfr(int connfd, int datafd, char *path, char *fr)
{
    printf("Rename from %s\n\r", path);
    ftpStorageRnfr(connfd, datafd, path);
    strcpy(fr, path);
    close(datafd);
    return 0;
}
int ftpProcessRnto(int connfd, int datafd, char *path, char *fr)
{
#if defined(VERBOSE)
    printf("to %s\n\r", path);
#endif
    ftpStorageRnto(connfd, datafd, fr, path);
    close(datafd);
    return 0;
}

int ftpProcessList(int connfd, int datafd, char *path)
{
    char dir[MAXBUFF];
    if(path == NULL){
        dir[0] = '\0';
    }
    else{
        strcpy(dir, path);
    }
#if defined(VERBOSE)
    printf("Listing \"%s\"\n\r", dir);
#endif
#if defined(TEST)
    send(connfd, RESP_125_TRANS_START, strlen(RESP_125_TRANS_START), 0);
    send(datafd, "-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n", strlen("-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n"), 0);
    send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0);
    return 0;
#else
    ftpStorageList(connfd, datafd, dir);
#endif
    close(datafd);
    return 0;
}
int ftpProcessPwd(int connfd)
{
    return ftpStoragePwd(connfd);
}
int ftpProcessCwd(int connfd, char *arg)
{
    char path[MAXPATH];
    if(arg == NULL){
        path[0] = '\0';
    }
    else{
        strcpy(path, arg);
    }
#if defined(VERBOSE)
    printf("Cwd to %s\n\r", path);
#endif
    return ftpStorageCwd(connfd, path);
}
int ftpProcessMkd(int connfd, int datafd, char *path)
{
#if defined(VERVOSE)
    printf("Creating directory %s\n\r", path);
#endif
    ftpStorageMkd(connfd, datafd, path);
    close(datafd);
    return 0;
}
int ftpProcessRmd(int connfd, int datafd, char *path)
{
#if defined(VERVOSE)
    printf("Deleting directory %s\n\r", path);
#endif
    ftpStorageRmd(connfd, datafd, path);
    close(datafd);
    return 0;
}

// @@ type code does not change any behavior now
int ftpProcessType(int connfd, char *arg, OUTPUT char *typeCode)
{
    int retVal;
    strncpy(typeCode, arg, MAXPASS);
#if defined(VERBOSE)
    printf("Type code: %s\n\r", arg);
#endif
    // always pass now
    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("200 reply errror %d\n\r", retVal);
        return -1;
    }
    return 0;
}
int ftpProcessSyst(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_215_SYS_TYPE, strlen(RESP_215_SYS_TYPE), 0)) < 0){
        printf("215 reply errror %d\n\r", retVal);
        return -1;
    }
    return 0;
}
int ftpProcessPasv(int connfd, unsigned short portScan, unsigned long srvIp, int *datafd)
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
    tv.tv_sec = DATA_SOCK_TIMEOUT;
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
int ftpProcessNoop(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("send 200 Error %d\n\r", retVal);
        return -1;
    }
#if defined(VERBOSE)
    printf("Noop\n\r");
#endif
    return 0;
}
int ftpProcessInvalid(int connfd)
{
    int retVal;
    if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
        printf("send 503 Error %d\n\r", retVal);
        return -1;
    }
    return 0;
}

// not used
void ftpProcessPort(int connfd, unsigned long *cltIp, unsigned long *cltPort)
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
int  ftpSetupDataConnActive(int *datafd, unsigned long cltIp, unsigned short cltPort, unsigned short portScan)
{
    int retVal;
    struct sockaddr_in cliaddr, tempaddr;
    struct timeval tv;

	if ( (*datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Open datafd error");
    	return -1;
    }
    tv.tv_sec = CONN_SOCK_TIMEOUT;
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
