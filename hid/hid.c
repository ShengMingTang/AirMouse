// Standard includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
// simplelink includes 
#include "simplelink.h"
#include "wlan.h"
// Common interface includes
#include "common.h" 
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
#include "bma222.h"

// extern
extern unsigned long  g_ulStaIp;
extern volatile unsigned long  g_ulStatus;

// exported var
short int sensorM[3][3]; // arranged in [acc, gyro, mag][x, y, z]
SemaphoreHandle_t semHidKickStarter;

// Base address for first timer
static volatile unsigned long g_ulImuTimer = TIMERA0_BASE;
static signed char aucRdDataBuf[32];
static SemaphoreHandle_t semImuReady;

static void ImuTimerIntHandler(void)
{
    unsigned char ucRegOffset = 0x02;
    unsigned char ucRdLen = 7;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    I2C_IF_Write(BMA222_TWI_ADDR,&ucRegOffset,1,0);
    
    //
    // Read the specified length of data
    //
    I2C_IF_Read(BMA222_TWI_ADDR, &aucRdDataBuf[0], ucRdLen);
    sensorM[ACC][X] = aucRdDataBuf[1];
    sensorM[ACC][Y] = aucRdDataBuf[3];
    sensorM[ACC][Z] = aucRdDataBuf[5];
    xSemaphoreGiveFromISR(semImuReady, xHigherPriorityTaskWoken); // @@the NULL is okay after v.7.3.0
    Timer_IF_InterruptClear(g_ulImuTimer);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void hidInit()
{
    if((semHidKickStarter = xSemaphoreCreateCounting(1, 0)) == NULL){
        printf("Sem Hid Error\n\r");
    }
    if((semImuReady = xSemaphoreCreateCounting(1, 0)) == NULL){
        printf("Sem IMU Error\n\r");
    }
    I2C_IF_Open(I2C_MASTER_MODE_FST);
    Timer_IF_Init(PRCM_TIMERA0, g_ulImuTimer, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(g_ulImuTimer, TIMER_A, ImuTimerIntHandler);
    Timer_IF_Start(g_ulImuTimer, TIMER_A, SAMPLE_PERIOD_IN_MS);
}
void hidTask(void *pvParameters)
{
    int fd;
    struct sockaddr_in	servaddr;
    short int buff[MAXBUFF];
    // char buff[MAXBUFF];
    int retVal;

    while(1){
        xSemaphoreTake(semHidKickStarter, portMAX_DELAY); // wait for IP layer task go first

        // init server addr
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family      = AF_INET;
        servaddr.sin_addr.s_addr = htonl(g_ulStaIp);
        servaddr.sin_port        = htons((unsigned short)RECVR_PORT);

        if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            printf("open sender sock Error %d\n\r", fd);
            continue;
        }
        
        if((retVal = connect(fd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0){
            printf("Hid connect Error %d retry every second\n\r", retVal);
            close(fd);
            continue;
        }
        printf("Hid connected\n\r");

        while(1){
            xSemaphoreTake(semImuReady, portMAX_DELAY);
            buff[0] = sensorM[ACC][0];
            buff[1] = sensorM[ACC][1];
            buff[2] = sensorM[ACC][2];
            if((retVal = send(fd, buff, 3*sizeof(short int), 0)) < 0){
                printf("Error on Send %d\n\r", retVal);
                break;
            }
            // snprintf(buff, sizeof(buff), "%d %d %d",
            //     sensorM[ACC][0], sensorM[ACC][1], sensorM[ACC][2]);
            // if((retVal = send(fd, buff, strlen(buff), 0)) < 0){
            //     printf("Error on Send %d\n\r", retVal);
            //     break;
            // }
        }
        close(fd);
    }
}
