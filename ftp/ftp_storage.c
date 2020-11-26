#ifdef __cplusplus
extern "C"{
#endif
#include "ftp_server.h"
#include <ff.h>
static FATFS fs;
static const char month[][4] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Ago",
    "Sep", "Oct", "Nov", "Dec"
};
static void trimFilename(const char *p, char *trim);
#ifdef __cplusplus
}
#endif

static void trimFilename(const char *p, char *trim)
{
    int i1, i2;

    i1 = strlen(p);
    while(i1 >= 0 && p[i1] != '.') i1--;
    if(i1 < 0) i1 = strlen(p);

    for(i2 = 0; i2 < i1 && i2 < 8; i2++){
        trim[i2] = p[i2];
    }
    if(p[i1] == '.'){
        i1++;
        trim[i2++] = '.';
    }

    for(; p[i1] != '\0' && i2 < 12; i1++, i2++){
        trim[i2] = p[i1];
    }
    trim[i2] = '\0';
}

#if defined(PIPELINED)
void ftpStreamTask(void *pvParameters)
{
    FileStream_t *p = (FileStream_t*)pvParameters;
    EventBits_t bits;
    FRESULT res = FR_OK;

    while(!f_eof(p->fp) && (res == FR_OK)){
        // send/recv is completed
        bits = xEventGroupWaitBits(p->event, FILSTR_SOCK_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
        if((res = p->f(p->fp, p->buff, MAXTRANS, &(p->btf))) != FR_OK){
            printf("f Error %d\n\r", res);
            break;
        }
        xEventGroupSetBits(p->event, FILSTR_DISK_DONE);
        f_sync(p->fp);
    }
    // no more to transmit, wait for last sock done
    bits = xEventGroupWaitBits(p->event, FILSTR_SOCK_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    // tell sock that eof has been reached
    xEventGroupSetBits(p->event, FILSTR_EOF);
    // vTaskDelete(NULL);
    while(1);
}
#endif
int ftpStorageInit()
{
    FRESULT res = FR_NOT_READY;
    if((res = f_mount(&fs, "0", 1)) != FR_OK){ // immediately mounted
        printf("mount Error %d\n\r", res);
        return -1;
    }
    return 0;
}
// assume fs is mounted before all these functions are called
int verifyUserPass(char *user, char *pass)
{
    return 0;
}

int ftpStorageRetr(int connfd, int datafd, char *path)
{
    FIL fp;
    int retVal;
    // simple loop
#ifndef PIPELINED
    FRESULT res;
    char buff[MAXTRANS];
    UINT btf;
#else
    TaskHandle_t child;
    FileStream_t param;
    EventBits_t bits;
    char db[2][MAXTRANS];
    int which = 0;
    int btf;
#endif

    // sanity check
    if((res = f_open(&fp, path, FA_READ)) != FR_OK){
        printf("Open %s Error %d\n\r", path, res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    if((retVal = send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0)) < 0){
        printf("send 150 Error %d\n\r", retVal);
        return -1;
    }

#ifndef PIPELINED
    retVal = 0;
    res = FR_OK;
    while(!f_eof(&fp) && (retVal >= 0) && (res == FR_OK)){
        if((res = f_read(&fp, buff, sizeof(buff), &btf)) != FR_OK){
            printf("f_read Error %d\n\r", res);
            break;
        }
        if((retVal = send(datafd, buff, btf, 0)) < 0){
            if(retVal != EAGAIN){
                printf("send %d bytes Error %d\n\r", btf, retVal);
                break;
            }
            // EAGAIN is Okay
            // else{} is not neccessary
        }
    }
#else
    if(xTaskCreate(ftpStreamTask, "", STR_STCK_SIZE, (void*)(&param), STR_TASK_PRIOR, &child) != pdPASS){
        printf("Create streaming task Error\n\r");
        return -1;
    }

    param.fp = &fp;
    param.f = f_read; // function callback
    param.buff = db[which];
    if((param.event = xEventGroupCreate()) == NULL){
        printf("Create Event group Error\n\r");
    }

    // disk should be started first
    xEventGroupSetBits(param.event, FILSTR_SOCK_DONE);
    while(1){
        bits = xEventGroupWaitBits(param.event, FILSTR_DISK_DONE | FILSTR_EOF, pdTRUE, pdFALSE, pdMS_TO_TICKS(FILSTR_EVENT_WAIT_MS));
        if(bits & FILSTR_DISK_DONE){ // disk has finished its work
            // prepare for next disk io
            btf = param.btf;
            param.buff = db[!which];
            // printf("3\n\r"); // @@ debug
            xEventGroupSetBits(param.event, FILSTR_SOCK_DONE);
            // socket part
            if((retVal = send(datafd, db[which], btf, 0)) < 0){
                printf("Send Error %d\n\r", retVal);
                break;
            }
            else{
                // prepare for next disk io
                which = !which;
            }
            // printf("4\n\r"); // @@ debug
        }
        // eof has been reached, break out loop 
        else if(bits & FILSTR_EOF){
            break;
        }
        else{ // none of above
            printf("timeout in disk I/O\n\r");
            break;
        }
    }
    vTaskDelete(child);
    vEventGroupDelete(param.event);
#endif

    f_close(&fp);
    if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("Send 226 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
int ftpStorageStor(int connfd, int datafd, char *path)
{
    FIL fp;
    int retVal;

#ifndef PIPELINED
    FRESULT res;
    char buff[MAXTRANS];
    char fn[MAXPATH];
    UINT btf;
#else
    TaskHandle_t child;
    FileStream_t param;
    EventBits_t bits;
    char db[2][MAXTRANS];
    int which = 0;
    int btf;
#endif

    // sanity check
    trimFilename(path, fn);
    printf("Filename %s trimmed to %s\n\r", path, fn);
    if(f_open(&fp, fn, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK){
        printf("Open %s Error %d\n\r", fn);
        if((retVal = send(connfd, RESP_451_REQ_ABR, strlen(RESP_451_REQ_ABR), 0)) < 0){
            printf("Send 451 Error %d\n\r", retVal);
        }
        fclose(&fp);
        return -1;
    }
    if((retVal = send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0)) < 0){
        printf("send 150 Error %d\n\r", retVal);
        fclose(&fp);
        return -1;
    }

#ifndef PIPELINED
    retVal = 0;
    res = FR_OK;

    while(((retVal = recv(datafd, buff, sizeof(buff), 0)) > 0) && (res == FR_OK)){
        if((res = f_write(&fp, "abc", sizeof("abc"), &btf)) != FR_OK){
            printf("f_write Error %d, tried to write %d bytes\n\r", res, retVal);
            break;
        }
    }
#else
    if(xTaskCreate(ftpStreamTask, "", STR_STCK_SIZE, (void*)(&param), STR_TASK_PRIOR, &child) != pdPASS){
        printf("Create streaming task Error\n\r");
        return -1;
    }

    param.fp = &fp;
    param.f = f_read; // function callback
    param.buff = db[which];
    if((param.event = xEventGroupCreate()) == NULL){
        printf("Create Event group Error\n\r");
    }

    // disk should be started first
    xEventGroupSetBits(param.event, FILSTR_SOCK_DONE);
    while(1){
        bits = xEventGroupWaitBits(param.event, FILSTR_DISK_DONE | FILSTR_EOF, pdTRUE, pdFALSE, pdMS_TO_TICKS(FILSTR_EVENT_WAIT_MS));
        if(bits & FILSTR_DISK_DONE){ // disk has finished its work
            // prepare for next disk io
            btf = param.btf;
            param.buff = db[!which];
            xEventGroupSetBits(param.event, FILSTR_SOCK_DONE);
            // socket part
            if((retVal = send(datafd, db[which], btf, 0)) < 0){
                printf("Send Error %d\n\r", retVal);
                break;
            }
            else{
                // prepare for next disk io
                which = !which;
            }
        }
        // eof has been reached, break out loop 
        else if(bits & FILSTR_EOF){
            break;
        }
        else{ // none of above
            printf("timeout in disk I/O\n\r");
            break;
        }
    }
    vTaskDelete(child);
    vEventGroupDelete(param.event);
#endif

    if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("Send 226 Error %d\n\r", retVal);
    }
    f_close(&fp);

    printf("Finishing storing %s\n\r", path);
    return 0;
}
int ftpStorageDele(int connfd, int datafd, char *path)
{
    FILINFO fno;
    FRESULT res;
    int retVal;
    
    // sanity check
    if(f_stat(path, &fno) != FR_OK){
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }

    if((res = f_unlink(path)) != FR_OK){
        printf("Delete Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }

    if((retVal = send(connfd, RESP_250_FILE_ACT_OK, strlen(RESP_250_FILE_ACT_OK), 0)) < 0){
        printf("Send 250 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
int ftpStorageRnfr(int connfd, int datafd, char *path)
{
    FILINFO fno;
    int retVal;
    
    // sanity check
    if(f_stat(path, &fno) != FR_OK){
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    if((retVal = send(connfd, RESP_350_FILE_ACT_PEND, strlen(RESP_350_FILE_ACT_PEND), 0)) < 0){
        printf("Send 350 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
int ftpStorageRnto(int connfd, int datafd, char *path, char *npath)
{
    FRESULT res;
    int retVal;
    char fn[MAXPATH];
    
    trimFilename(npath, fn);
    if((res = f_rename(path, fn)) != FR_OK){
        printf("rename Error %d\n\r", res);
        if((retVal = send(connfd, RESP_553_REQ_NOT_TAKEN_FN_NOT_ALLOWED, strlen(RESP_553_REQ_NOT_TAKEN_FN_NOT_ALLOWED), 0)) < 0){
            printf("Send 553 Error %d\n\r", retVal);
        }
        return -1;
    }

    if((retVal = send(connfd, RESP_250_FILE_ACT_OK, strlen(RESP_250_FILE_ACT_OK), 0)) < 0){
        printf("Send 250 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}

int ftpStorageList(int connfd, int datafd, char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char buff[MAXBUFF];
    int retVal;

    if((res = f_opendir(&dir, path)) != FR_OK){
        printf("Open %s failed with Error %d\n\r", path, res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("send 550 for list Error %d\n\r", retVal);
        }
        return -1;
    }
    
    if((retVal = send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0)) < 0){
        printf("send 150 Error %d\n\r", retVal);
        return -1;
    }
    while(1){
        res = f_readdir(&dir, &fno);
        if((res != FR_OK) || fno.fname[0] == '\0'){ // all items are visited
            break;
        }
        snprintf(buff, sizeof(buff), "%crw-rw-rw- root root %lu %s %d %d:%d %s\r\n",
            (fno.fattrib & AM_DIR)? 'd' : '-',
            fno.fsize, month[(fno.fdate >> 5) & 0x0F], fno.fdate & 0x1F, // month date
            (fno.ftime >> 11) & 0x1F, (fno.ftime >> 5) & 0x3F, // hour:min
            fno.fname
        );
        if((retVal = send(datafd, buff, strlen(buff), 0)) < 0){
            printf("Send %s Error %d\n\r", buff, retVal);
        }
    }

    f_closedir(&dir);
    if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("Send 226 Error %d\n\r", retVal);
    }

    return 0;
}
int ftpStoragePwd(int connfd)
{
    int retVal;
    char resp[MAXBUFF];
    char dir[MAXBUFF];
    
    // always pass now
    f_getcwd(dir, sizeof(dir));

    sprintf(resp, "257 \"%s\"created\r\n", dir);
    if((retVal = send(connfd, resp, strlen(resp), 0)) < 0){
        printf("257 reply errror %d\r\n", retVal);
        return -1;
    }
    return 0;
}
int ftpStorageCwd(int connfd, char *path)
{
    FRESULT res;
    int retVal;
    // always pass now
    if((res = f_chdir(path)) != FR_OK){
        printf("chdir Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("550 reply errror %d\r\n", retVal);
        }
        return -1;
    }
    if((retVal = send(connfd, RESP_250_FILE_ACT_OK, strlen(RESP_250_FILE_ACT_OK), 0)) < 0){
        printf("250 reply errror %d\r\n", retVal);
        return -1;
    }
    return 0;
}
int ftpStorageMkd(int connfd, int datafd, char *path)
{
    FILINFO fno;
    FRESULT res;
    int retVal;
    char buff[MAXBUFF] = {0};
    
    // sanity check
    if((res = f_stat(path, &fno)) == FR_OK){
        printf("Stat Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    
    trimFilename(path, buff);
    if((res = f_mkdir(buff)) != FR_OK){
        printf("Create dir Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    sprintf(buff, "257 %s created\r\n", path);
    if((retVal = send(connfd, buff, strlen(buff), 0)) < 0){
        printf("Send 257 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
int ftpStorageRmd(int connfd, int datafd, char *path)
{
    FILINFO fno;
    FRESULT res;
    int retVal;
    
    // sanity check
    if((res = f_stat(path, &fno)) != FR_OK){
        printf("Stat Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }

    if((res = f_unlink(path)) != FR_OK){
        printf("Deleting dir Error %d\n\r", res);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    if((retVal = send(connfd, RESP_250_FILE_ACT_OK, strlen(RESP_250_FILE_ACT_OK), 0)) < 0){
        printf("Send 250 Error %d\n\r", retVal);
        return -1;
    }

    return 0;
}
