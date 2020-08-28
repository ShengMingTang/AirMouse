#include "ftp_server.h"
#include <ff.h>
static const char month[][4] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Ago",
    "Sep", "Oct", "Nov", "Dec"
};

void ftpStreamTask(void *pvParameters)
{
    FileStream_t *p = (FileStream_t*)pvParameters;
    EventBits_t bits;
    FRESULT res = FR_OK;

    while(!f_eof(p->fp) && (res == FR_OK)){
        // send/recv is completed
        bits = xEventGroupWaitBits(p->event, FILSTR_SOCK_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
        printf("I'm disk!\n\r"); // @@ debug
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
// assume fs is mounted before all these functions are called
int ftpStorageList(int connfd, int datafd, char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char buff[MAXBUFF];
    int retVal;

    if((res = f_opendir(&dir, path)) != FR_OK){
        printf("Open %s failed\n\r", path);
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

int ftpStorageRetr(int connfd, int datafd, char *path)
{
    FIL fp;
    int retVal;

    // sanity check
    if(f_open(&fp, path, FA_READ) != FR_OK){
        printf("Open %s Error %d\n\r", path);
        if((retVal = send(connfd, RESP_550_REQ_NOT_TAKEN, strlen(RESP_550_REQ_NOT_TAKEN), 0)) < 0){
            printf("Send 550 Error %d\n\r", retVal);
        }
        return -1;
    }
    if((retVal = send(connfd, RESP_150_FILE_STAT_OK, strlen(RESP_150_FILE_STAT_OK), 0)) < 0){
        printf("send 150 Error %d\n\r", retVal);
        return -1;
    }

    // non-pipelining
    // FRESULT res;
    // char buff[MAXTRANS];
    // UINT btf;
    // retVal = 0;
    // res = FR_OK;
    // while(!f_eof(&fp) && (retVal >= 0) && (res == FR_OK)){
    //     if((res = f_read(&fp, buff, sizeof(buff), &btf)) != FR_OK){
    //         printf("f_read Error %d\n\r", res);
    //         continue;
    //     }
    //     if((retVal = send(datafd, buff, btf, 0)) < 0){
    //         printf("send %d bytes Error %d\n\r", btf, retVal);
    //         vTaskDelay(pdMS_TO_TICKS(SOCK_BREAK_MS));
    //     }
    // }

    // pipelining
    TaskHandle_t child;
    FileStream_t param;
    EventBits_t bits;
    char db[2][MAXTRANS];
    int which = 0;
    int btf;

    // create a child task
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
        bits = xEventGroupWaitBits(param.event, FILSTR_DISK_DONE | FILSTR_EOF, pdTRUE, pdFALSE, pdMS_TO_TICKS(FILSTR_EVENT_WAIT_MS)/*portMAX_DELAY*/);
        if(bits & FILSTR_DISK_DONE){ // disk has finished its work
            printf("I'm socket!\n\r"); // @@ debug
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

    f_close(&fp);
    vTaskDelete(child);
    vEventGroupDelete(param.event);
    if((retVal = send(connfd, RESP_226_TRANS_COMPLETE, strlen(RESP_226_TRANS_COMPLETE), 0)) < 0){
        printf("Send 226 Error %d\n\r", retVal);
    }
    printf("Finishing retriveing %s\n\r", path);

    return 0;
}
