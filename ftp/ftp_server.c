#ifdef __cplusplus
extern "C"{
#endif

#include "ftp_server.h"

#ifdef __cplusplus
}
#endif
static int ind[MAX_NUM_TASKS]; // indicies 
static volatile int socks[MAX_NUM_TASKS] = {0}; // the ith task accesses socks[i]
void ftpServerInit()
{
    while(ftpStorageInit() < 0){
        printf("Storage Init Error\n\r");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
void ftpServerTask(void *pvParameters)
{
    // socket
    int	listenfd, connfd;
    struct sockaddr_in	servaddr;
    // local
    int retVal;
    int i;

    ftpServerInit();
    // create all tasks
    for(i = 0; i < MAX_NUM_TASKS; i++){
        ind[i] = i;
        if((retVal = xTaskCreate(ftpConnTask, "", CONN_STACK_SIZE, (void*)(&(ind[i])), DATA_TASK_PRIOR, NULL)) != pdPASS){
            printf("Create Conn task Error %d\n\r", retVal);
            while(1) {} // stuck
        }
    }

    // init server addr
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons((unsigned short)FTP_PORT);
    // open listening socket
    while((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("open FTP Socket Error %d\r\n", listenfd);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    // bind
    while(retVal = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
        printf("bind Socket Error %d\r\n", retVal);
        // close(listenfd);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    while(retVal = listen(listenfd, LISTENQ) < 0){
        printf("listen Socket Error %d\r\n", retVal);
        // close(listenfd);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // wait for connection and kickstart subtasks or reject
    while(1){
        // ready to accept connection
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        // check connfd is valid
        if(connfd < 0){
            printf("accept Socket error %d, continue listening\n\r", connfd);
        }
        else{ // on receiving a connection request
            #if defined(VERBOSE)
                printf("New Client Detected ...\n\r");
            #endif
            i = 0;
            while(socks[i] && i < MAX_NUM_TASKS) {i++;} // i is the first available task
            if(i == MAX_NUM_TASKS){ // no available task
                if((retVal = send(connfd, RESP_421_SRV_NOT_AVAI, strlen(RESP_421_SRV_NOT_AVAI), 0)) < 0){
                    printf("Send Load full Error %d\n\r", retVal);
                }
            }
            else{ // kickstart task i
                socks[i] = connfd;
            }            
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void ftpConnTask(void *pvParameters)
{
    // socket
    int connfd;
    int datafd;
    struct timeval tv;
#if defined(SUPPORT_ACTIVE_CONN)
    unsigned long cltIp, cltPort;
#endif
    unsigned long portScan; // port scan from 5001 down ward
    // command
    char recvline[MAXLINE];
    char *scmd, *arg;
    Cmd_t cmd = CMD_INVALID;
    // user data
    char user[MAXUSERNAME];
    char typeCode[MAXLINE];
    char rn[MAXPATH];
    // local
    int retVal;
    int done = 0; // 0 for enter loop, 1 on break the loop
    int isConnOpen = 0; // 1 for data connection is open
    int ind = *((int*)pvParameters); // index to socks array
    
    // main loop
    while(1){
        done = 0, isConnOpen = 0;
        datafd = -1, portScan = 5001;
        connfd = socks[ind];
        if(connfd == 0){ // no work, keep waiting
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        // kickstarted
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
            if((retVal = recv(connfd, recvline, sizeof(recvline), 0)) < 0){
                if(retVal == EAGAIN){ // timedout
                    printf("Connection timeout leave\n\r");
                    break;
                }
                else{ // unrecoverable error
                    printf("Conn recv Error %d\n\r", retVal);
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
            printf("cmd: %d\n\r", cmd);
            switch (cmd){
                /* user */
                case CMD_USER:{
                    if(ftpProcessUser(connfd, arg, user))
                        done = 1;
                    break;
                }
                case CMD_PASS:{
                    if(ftpProcessPass(connfd, user, arg))
                        done = 1;
                    break;
                }
                case CMD_QUIT:{ // exit anyway
                    ftpProcessQuit(connfd);
                    done = 1;
    #if defined(VERBOSE)
                    printf("%s quits\n\r", user);
    #endif
                    break;
                }
                /* file operation */
                case CMD_RETR:{
                    if(isConnOpen){
                        isConnOpen = 0;
                        if(ftpProcessRetr(connfd, datafd, arg))
                            done = 1;
                    }
                    else{
                        #if defined(FTP_AUTH_BYPASS)
                            isConnOpen = 0;
                            if(ftpProcessRetr(connfd, datafd, arg))
                                done = 1;
                        #else
                            printf("Conn not open\n\r");
                            if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                            printf("Send 503 Error %d\n\r", retVal);
                            done = 1;
                            }
                        #endif
                    }
                    break;
                }
                case CMD_STOR:{
                    if(isConnOpen){
                        isConnOpen = 0;
                        if(ftpProcessStor(connfd, datafd, arg))
                            done = 1;
                    }
                    else{
                        #if defined(FTP_AUTH_BYPASS)
                            isConnOpen = 0;
                            if(ftpProcessStor(connfd, datafd, arg))
                                done = 1;
                        #else
                            printf("Conn not open\n\r");
                            if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                            printf("Send 503 Error %d\n\r", retVal);
                            done = 1;
                            }
                        #endif
                    }
                    break;
                }
                case CMD_DELE:{
                    if(ftpProcessDele(connfd, datafd, arg))
                        done = 1;
                    break;
                }
                case CMD_RNFR:{
                    if(ftpProcessRnfr(connfd, datafd, arg, rn))
                        done = 1;
                    break;
                }
                case CMD_RNTO:{
                    if(ftpProcessRnto(connfd, datafd, arg, rn))
                        done = 1;
                    break;
                }
                /* direcotry */
                case CMD_LIST:{
                    if(isConnOpen){
                        isConnOpen = 0;
                        if(ftpProcessList(connfd, datafd, arg))
                            done = 1;
                    }
                    else{
                        #if defined(FTP_AUTH_BYPASS)
                            isConnOpen = 0;
                            if(ftpProcessList(connfd, datafd, arg))
                                done = 1;
                        #else
                            printf("Conn not open\n\r");
                            if((retVal = send(connfd, RESP_503_BAD_SEQ, strlen(RESP_503_BAD_SEQ), 0)) < 0){
                            printf("Send 503 Error %d\n\r", retVal); 
                            done = 1;
                            }
                        #endif
                    }
                    break;
                }
                case CMD_PWD:{
                    if(ftpProcessPwd(connfd))
                        done = 1;
                    break;
                }
                case CMD_CWD:{
                    if(ftpProcessCwd(connfd, arg))
                        done = 1;
                    break;
                }
                case CMD_MKD:{
                    if(ftpProcessMkd(connfd, datafd, arg))
                        done = 1;
                    break;
                }
                case CMD_RMD:{
                    if(ftpProcessRmd(connfd, datafd, arg))
                        done = 1;
                    break;
                }
                /* common */
                case CMD_TYPE:{
                    if(ftpProcessType(connfd, arg, typeCode))
                        done = 1;
                    break;
                }
                case CMD_SYST:{
                    if(ftpProcessSyst(connfd))
                        done = 1;
                    break;
                }
                #if defined(SUPPORT_ACTIVE_CONN)
                    case CMD_PORT:{
                        if(ftpProcessPort(connfd, arg, &cltIp, &cltPort)){
                            done = 1;
                        }
                        else{
                            if(ftpSetupDataConnActive(&datafd, cltIp, cltPort, portScan))
                                done = 1;
                            else
                                isConnOpen = 1;
                        }
                        break;
                    }
                #endif
                case CMD_PASV:{
                    if(ftpProcessPasv(connfd, portScan, g_ulDeviceIp, &datafd)){
                        done = 1;
                    }
                    else{
                        isConnOpen = 1;
                    }
                    break;
                }
                case CMD_NOOP:{
                    if(ftpProcessNoop(connfd))
                        done = 1;
                    break;
                }
                case CMD_INVALID:{
                    if(ftpProcessInvalid(connfd) < 0)
                        done = 1;
                    break;
                }
                default:
                    break;
            }

        } // end of !done loop
        // tell parent that is task is done
        socks[ind] = 0;
        close(connfd);
        if(datafd > 0) close(datafd);
    } // end of main loop
}
int ftpGetCmd(char *str, Cmd_t *cmd)
{
    #warning "Case Insensitive command comparison is not implemented"
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
    else if(!strcmp(str, "PORT"))
        *cmd = CMD_PORT;
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
    // verification is defered until PASS is sent
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
    int retVal;
#if defined(VERBOSE)
    printf("Retrieving \"%s\"\n\r", path);
#endif

#if defined(TEST)
    int i;
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
    close(datafd);
#else
    retVal = ftpStorageRetr(connfd, datafd, path);
#endif
    close(datafd);
    return retVal;
}
int ftpProcessStor(int connfd, int datafd, char *path)
{
    int retVal;
#if defined(VERBOSE)
    printf("Storing %s\n\r", path);
#endif

#if defined(TEST)
    int i;
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
    if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("Test send Error %d\n\r", retVal);
    }
#else
    retVal = ftpStorageStor(connfd, datafd, path);
#endif
    close(datafd);
    return retVal;
}
int ftpProcessDele(int connfd, int datafd, char *path)
{
    int retVal;
#if defined(VERBOSE)
    printf("Deleting %s\n\r", path);
#endif
    retVal = ftpStorageDele(connfd, datafd, path);
    close(datafd);
    return retVal;
}
int ftpProcessRnfr(int connfd, int datafd, char *path, char *fr)
{
    int retVal;
    printf("Rename from %s\n\r", path);
    retVal = ftpStorageRnfr(connfd, datafd, path);
    strcpy(fr, path);
    close(datafd);
    return retVal;
}
int ftpProcessRnto(int connfd, int datafd, char *path, char *fr)
{
    int retVal;
#if defined(VERBOSE)
    printf("to %s\n\r", path);
#endif
    retVal = ftpStorageRnto(connfd, datafd, fr, path);
    close(datafd);
    return retVal;
}

int ftpProcessList(int connfd, int datafd, char *path)
{
    int retVal;
    char dir[MAXBUFF];
    if(path == NULL){
        dir[0] = '.';
        dir[1] = '\0';
    }
    else{
        strcpy(dir, path);
    }
#if defined(VERBOSE)
    printf("Listing \"%s\"\n\r", dir);
#endif
#if defined(TEST)
    if((retVal = send(connfd, RESP_125_TRANS_START, strlen(RESP_125_TRANS_START), 0)) < 0){
        printf("send 125 Error %d\n\r", retVal);
    }
    else if((retVal = send(datafd, "-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n", strlen("-rw-r--r-- 1 owner group           62914560 Aug 26 16:31 README.txt\r\n"), 0)) < 0){
        printf("send list Error %d\n\r", retVal);
    }
    else if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("send 226 Error %d\n\r", retVal);
    }
#else
    retVal = ftpStorageList(connfd, datafd, dir);
#endif
    close(datafd);
    return retVal;
}
int ftpProcessPwd(int connfd)
{
    return ftpStoragePwd(connfd);
}
int ftpProcessCwd(int connfd, char *arg)
{
    char path[MAXPATH];
    if(arg == NULL)
        path[0] = '\0';
    else
        strcpy(path, arg);
#if defined(VERBOSE)
    printf("Cwd to %s\n\r", path);
#endif
    return ftpStorageCwd(connfd, path);
}
int ftpProcessMkd(int connfd, int datafd, char *path)
{
    int retVal;
#if defined(VERVOSE)
    printf("Creating directory %s\n\r", path);
#endif
    retVal = ftpStorageMkd(connfd, datafd, path);
    close(datafd);
    return retVal;
}
int ftpProcessRmd(int connfd, int datafd, char *path)
{
    int retVal;
#if defined(VERVOSE)
    printf("Deleting directory %s\n\r", path);
#endif
    retVal = ftpStorageRmd(connfd, datafd, path);
    close(datafd);
    return retVal;
}

#warning "TYPE command is bypassed, having no effect"
int ftpProcessType(int connfd, char *arg, char *typeCode)
{
    int retVal;

    strncpy(typeCode, arg, MAXPASS);
#if defined(VERBOSE)
    printf("Type code: %s\n\r", arg);
#endif

    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("200 reply errror %d\n\r", retVal);
        return -1;
    }
    return 0;
}
#warning "SYST command is always answering UNIX style"
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
    int nonblocking = 1, count = 0;
    struct timeval tv;
    char resp[MAXBUFF];

	if ( (*datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Open datafd error %d", *datafd);
    	return -1;
    }
    tv.tv_sec = DATA_SOCK_TIMEOUT;
    tv.tv_usec = 0;
    if((retVal = setsockopt(*datafd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
        printf("Set socket rcvtimeo Error %d\n\r", retVal);
        close(*datafd);
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
        close(*datafd);
        return -1;
    }

    printf("use port %d to accept connection\n\r", portScan);
    if((retVal = listen(*datafd, LISTENQ)) < 0){ // fail
        printf("listen data socket Error %d\n\r", retVal);
        if((retVal = send(connfd, RESP_421_SRV_NOT_AVAI, strlen(RESP_421_SRV_NOT_AVAI), 0)) < 0){
            printf("send 421 Error %d\n\r", retVal);
        }
        close(*datafd);
        return -1;
    }
    else{ // success
        sprintf(resp, "227 Entering Passive Mode (%lu,%lu,%lu,%lu,%hu,%hu).\r\n", 
            (unsigned long)IPV4_BYTE(srvIp, 3), (unsigned long)IPV4_BYTE(srvIp, 2), (unsigned long)IPV4_BYTE(srvIp, 1), (unsigned long)IPV4_BYTE(srvIp, 0),
            (unsigned short)IPV4_BYTE(portScan, 1), (unsigned short)IPV4_BYTE(portScan, 0)
        );

        if(retVal = send(connfd, resp, strlen(resp), 0) < 0){
            printf("send 227 Error %d\n\r", retVal);
            close(*datafd);
            return -1;
        }
        else{ // accepting connection
            #if defined(VERBOSE)
                printf("resp: %s\n\r", resp);
                printf("accepting trans conn...\n\r");
            #endif
            // set nonblocking
            if((retVal = setsockopt(*datafd, SOL_SOCKET, SO_NONBLOCKING, (const char*)&nonblocking, sizeof(nonblocking))) < 0){
                printf("listenfd set nonblocking Error %d\n\r", retVal);
                close(*datafd);
                return -1;
            }
            while((transfd = accept(*datafd, (struct sockaddr*)NULL, NULL)) < 0 && count < PASV_TRY_TIMES){
                if(transfd != EAGAIN) printf("transfd Error %d\n\r", transfd);
                count++;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            if(transfd > 0){
                #if defined(VERBOSE)
                    printf("accepted\n\r");
                #endif
            }
            else{
                printf("accept failed %d\n\r", transfd);
                close(*datafd);
                return -1;
            }
        }
    }

    // success
    close(*datafd);
    *datafd = transfd;
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

#if defined(SUPPORT_ACTIVE_CONN)
#warning "Active connection is not tested"
int ftpProcessPort(int connfd, char *arg, OUTPUT unsigned long *cltIp, OUTPUT unsigned long *cltPort)
{
    int retVal;
    char *n1, *n2, *n3, *n4, *n5, *n6;
    char *p;

    // str = "ip[3],ip[2],ip[1],ip[0],port[1],port[0]"
    p = strtok(arg, "("); // ip3,ip2,ip1,ip0,port1,port0
	n1 = strtok(p, ","); // ip3
	n2 = strtok(NULL, ","); // ip2
	n3 = strtok(NULL, ","); // ip1
	n4 = strtok(NULL, ","); // ip0
	n5 = strtok(NULL, ","); // port1
	n6 = strtok(NULL, ")"); // port0

    *cltIp = (atoi(n1) << 24) | (atoi(n2) << 16) | (atoi(n3) << 8) | (atoi(n4));
    *cltPort = (atoi(n5) << 8) | (atoi(n6));
#if defined(VERBOSE)
    printf("Client Ip: %d.%d.%d.%d\n\r", 
        ((unsigned int)*cltIp & 0xFF000000) >> 24,
        ((unsigned int)*cltIp & 0x00FF0000) >> 16,
        ((unsigned int)*cltIp & 0x0000FF00) >> 8,
        ((unsigned int)*cltIp & 0x000000FF)
    );
    printf("Client Port: %d\n\r", *cltPort);
#endif

    if((retVal = send(connfd, RESP_200_CMD_OK, strlen(RESP_200_CMD_OK), 0)) < 0){
        printf("send PORT resp Error %d\n\r", retVal);
        return -1;
    }

    return 0;
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
        close(*datafd);
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

#if defined(VERBOSE)
    printf("use port %d to connect\n\r", portScan);
#endif

	//initiate data connection fd with client ip and client port             
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port   = htons(cltPort);
    cliaddr.sin_addr.s_addr = htonl((unsigned long)cltIp);

    if (connect(*datafd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0){
        printf("Connect to client Failed\n\r");
        close(*datafd);
    	return -1;
    }

    return 0;
}
#endif
