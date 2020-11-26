#ifdef __cplusplus
extern "C"{
#endif
// Standard includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_gpio.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "gpio.h"
#include "pin.h"
#include "uart.h"
#include "timer.h"
// simplelink includes 
#include "simplelink.h"
#include "wlan.h"
// Common interface includes
#include "common.h" 
#include "gpio_if.h"
#include "uart_if.h"
#include "timer_if.h"
#include "i2c_if.h"
// FreeRTOS includes
#include <FreeRTOS.h>
#include <projdefs.h>
#include <task.h>
#include <semphr.h>
#include <event_groups.h>
// custom includes
#include "hid.h"
#include "sensor.h"

static void ImuTimerIntHandler(void);

#ifdef __cplusplus
}
#endif

volatile unsigned long g_ulImuTimer = TIMERA0_BASE; // Base address for first timer
// extern
extern volatile unsigned long  g_ulStaIp;
extern volatile unsigned long  g_ulStatus;
extern void ImuTimerIntHandler(void);

void hidInit()
{
    I2C_IF_Open(I2C_MASTER_MODE_FST);
    Timer_IF_Init(PRCM_TIMERA0, g_ulImuTimer, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(g_ulImuTimer, TIMER_A, ImuTimerIntHandler);
    Timer_IF_Start(g_ulImuTimer, TIMER_A, SAMPLE_PERIOD_IN_MS);

    sensorInit();
}
void hidTask(void *pvParameters)
{
    int fd, tempfd;
    struct sockaddr_in	servaddr;
    int retVal;
    struct timeval tv;
    char buff[MAXBUFF];
    unsigned char ucRegOffset;
    unsigned char ucRdLen;

    hidInit();

    tv.tv_sec = HID_REPLY_TIMEOUT; // End device must reply within this more second
    tv.tv_usec = 0;

    // init server addr
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons((int)MOUSE_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    while(1){
        /* open hid socket, retry until success */
        do{
            vTaskDelay(pdMS_TO_TICKS(1000));
            // open socket      
            if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                printf("open sender sock Error %d\n\r", fd);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            // bind
            if(retVal = bind(fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
                printf("bind Socket Error %d\r\n", retVal);
                close(fd);
                continue;
            }
            // listen
            if(retVal = listen(fd, LISTENQ) < 0){
                printf("listen Socket Error %d\r\n", retVal);
                close(fd);
                continue;
            }
            // ready to accept connection and check
            tempfd = accept(fd, (struct sockaddr*)NULL, NULL);
            close(fd);
            if(tempfd < 0){
                printf("accept socket Error %d\n\r", tempfd);
                continue;
            }
            fd = tempfd;
            // set recv timeout
            if((retVal = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv))) < 0){
                printf("Set socket rcvtimeo Error %d\n\r", retVal);
                close(fd);
                continue;
            }
        }while(retVal < 0);
        printf("Hid connected\n\r");
        
        while(1){
            if((retVal = recv(fd, buff, sizeof(buff), 0)) < 0){
                printf("Error on Recv confirm %d\n\r", retVal);
                break;
            }
            memset(buff, 0, sizeof(buff));

            sensorRead();
            sensorUpdate();
            // fill in report
            sensorToReport(buff);

            if((retVal = send(fd, buff, 4 + KB_MAXNUM_KEY_PRESS, 0)) < 0){
                printf("%d Error on Send %d\n\r", fd, retVal);
                break;
            }
        }
        close(fd);
    }

    while(1) {}
}
