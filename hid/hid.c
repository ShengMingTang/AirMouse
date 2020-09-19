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

// extern
extern volatile unsigned long  g_ulStaIp;
extern volatile unsigned long  g_ulStatus;

// exported var
static volatile signed char sensorM[3]; // arranged in [acc, gyro, mag][x, y, z]
SemaphoreHandle_t semHidKickStarter;
// mouse
static char mousePress, lastMousePress, mouseClick;
// keyboard
static char kb[3+KB_MAXNUM_KEY_PRESS]; // 1(modifier) + 1(OEM reserved) + 1(LED) + max num key pressed

// Base address for first timer
static volatile unsigned long g_ulImuTimer = TIMERA0_BASE;
static volatile signed char aucRdDataBuf[32];
static SemaphoreHandle_t semImuReady;

static void ImuTimerIntHandler(void)
{
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static char debounceL, debounceR;
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    unsigned char ucPinValue;

    lastMousePress = mousePress;
    mousePress = 0;
    // mouseClick = 0; // cleared by host

    GPIO_IF_GetPortNPin(MOUSE_BTN_LEFT_PIN,&uiGPIOPort,&pucGPIOPin);
    ucPinValue = GPIO_IF_Get(MOUSE_BTN_LEFT_PIN,uiGPIOPort,pucGPIOPin);
    debounceL = (debounceL << 1) | ucPinValue;
    if(debounceL == 0xff){
        mousePress |= MOUSE_LEFT;
        if((mousePress & MOUSE_LEFT) != (lastMousePress & MOUSE_LEFT)){
            mouseClick |= MOUSE_LEFT;
        }
    }

    GPIO_IF_GetPortNPin(MOUSE_BTN_RIGHT_PIN,&uiGPIOPort,&pucGPIOPin);
    ucPinValue = GPIO_IF_Get(MOUSE_BTN_RIGHT_PIN, uiGPIOPort,pucGPIOPin);
    debounceR = (debounceR << 1) | ucPinValue;
    if(debounceR == 0xff){
        mousePress |= MOUSE_RIGHT;
        if((mousePress & MOUSE_RIGHT) != (lastMousePress & MOUSE_RIGHT)){
            mouseClick |= MOUSE_RIGHT;
        }
    }
    // if(mouseClick){
    //     printf("%d\n\r", mouseClick);
    // }
    
    // xSemaphoreGiveFromISR(semImuReady, &xHigherPriorityTaskWoken); // @@the NULL is okay after v.7.3.0
    Timer_IF_InterruptClear(g_ulImuTimer);
    // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void hidInit()
{
    if((semHidKickStarter = xSemaphoreCreateCounting(1, 0)) == NULL){
        printf("Sem Hid Error\n\r");
    }
    // if((semImuReady = xSemaphoreCreateCounting(1, 0)) == NULL){
    //     printf("Sem IMU Error\n\r");
    // }

    I2C_IF_Open(I2C_MASTER_MODE_FST);
    Timer_IF_Init(PRCM_TIMERA0, g_ulImuTimer, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(g_ulImuTimer, TIMER_A, ImuTimerIntHandler);
    Timer_IF_Start(g_ulImuTimer, TIMER_A, SAMPLE_PERIOD_IN_MS);
    
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


    tv.tv_sec = 2; // End device must reply within this more second
    tv.tv_usec = 0;

    // power up
    aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_1;
    aucRdDataBuf[1] = 0x20; // cycle mode
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],2,0);
    // power up check
    aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_1;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],1,0);
    aucRdDataBuf[0] = 0;
    I2C_IF_Read(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0],1);
    printf("Power check: %x", aucRdDataBuf[0]);
    // DLPF to 1 (184 Hz acc, 188Hz gyro)
    // aucRdDataBuf[0] = MPU6050_RA_CONFIG;
    // aucRdDataBuf[1] = 0x00;
    // I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],2,0);
    // div = gyro / 1
    // aucRdDataBuf[0] = MPU6050_RA_SMPLRT_DIV;
    // aucRdDataBuf[1] = 0x00;
    // I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],2,0);

    vTaskDelay(pdMS_TO_TICKS(500)); // little delay for hardware

    // while(1){
    //     ucRegOffset = 0x02;
    //     ucRdLen = 7;
        
    //     I2C_IF_Write(BMA222_TWI_ADDR,&ucRegOffset,1,0);
    //     I2C_IF_Read(BMA222_TWI_ADDR, &aucRdDataBuf[0], ucRdLen);
    //     // printf("BMA222:%f,%f,%f\n\r", (float)aucRdDataBuf[1]/64, (float)aucRdDataBuf[3]/64, (float)aucRdDataBuf[5]/64);
        
    //     ucRegOffset = MPU6050_RA_ACCEL_XOUT_H;
    //     ucRdLen = 14;
    //     I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&ucRegOffset,1,0);
    //     I2C_IF_Read(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], ucRdLen);
        
    //     int a, b, c;
    //     a = ((int)aucRdDataBuf[0] << 8 | aucRdDataBuf[1]);
    //     b = ((int)aucRdDataBuf[2] << 8 | aucRdDataBuf[3]);
    //     c = ((int)aucRdDataBuf[4] << 8 | aucRdDataBuf[5]);
        
    //     // printf("MPU:%f,%f,%f\n\r", (float)a/16384, (float)b/16384, (float)c/16384);
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }

    // init server addr
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons((int)MOUSE_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    while(1){
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
            ucRegOffset = 0x02;
            ucRdLen = 7;
            // 
            // Write the register address to be read from.
            // Stop bit implicitly assumed to be 0.
            //
            I2C_IF_Write(BMA222_TWI_ADDR,&ucRegOffset,1,0);
            
            //
            // Read the specified length of data
            //
            I2C_IF_Read(BMA222_TWI_ADDR, &aucRdDataBuf[0], ucRdLen);
            if((retVal = recv(fd, buff, sizeof(buff), 0)) < 0){
                printf("Error on Recv confirm %d\n\r", retVal);
                break;
            }
            // fill in btn, xyw, key pressed
            memset(buff, 0, sizeof(buff));
            buff[0] = mouseClick;
            buff[1] = -aucRdDataBuf[1];
            buff[2] = -aucRdDataBuf[3]; // y
            // buff[2] = (aucRdDataBuf[5] - 64) * 4; // z
            // keyboard will fill in later
            // 
            mouseClick = 0;

            if((retVal = send(fd, buff, 4 + KB_MAXNUM_KEY_PRESS, 0)) < 0){
                printf("%d Error on Send %d\n\r", fd, retVal);
                break;
            }
        }
        close(fd);
    }
}
