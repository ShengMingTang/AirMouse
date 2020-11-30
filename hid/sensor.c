#ifdef __cplusplus
extern "C"{
#endif
// Standard includes
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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
// sensor includes
#include "sensor.h"


#define UartPutChar(c)       MAP_UARTCharPut(CONSOLE,c)
#define UartGetChar()        MAP_UARTCharGet(CONSOLE)

extern char mouseClick;

static signed char  aucRdDataBuf[32];
static signed short sensorData[SENSOR_AXIS]; // {x, y, roll}

// HID report
extern volatile unsigned long g_ulImuTimer;
// mouse
static char mousePress, lastMousePress, mouseClick;
// keyboard
static char kb[3+KB_MAXNUM_KEY_PRESS]; // 1(modifier) + 1(OEM reserved) + 1(LED) + max num key pressed

#ifdef USE_MPU6050
    /* copied from https://github.com/n1rml/esp32_airmouse main*/
    #include "MPU6050.h"
#endif

#ifdef USE_MPU6050
    static const int sensorOffset[SENSOR_AXIS] = {0, 0, 67};
    static const int sensorThres[SENSOR_AXIS] = {5, 3, 5};
#else
    static const int sensorOffset[SENSOR_AXIS] = {0, 0, 67};
    static const int sensorThres[SENSOR_AXIS] = {5, 3, 5};
#endif

#ifdef __cplusplus
}
#endif

void sensorInit()
{
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    unsigned char ucPinValue;
    GPIO_IF_GetPortNPin(MOUSE_INPUT_SEL_PIN,&uiGPIOPort,&pucGPIOPin);

    ucPinValue = GPIO_IF_Get(MOUSE_INPUT_SEL_PIN,uiGPIOPort,pucGPIOPin);
    /* sensor init */
#ifdef USE_MPU6050
    /* mpu6050 */
    if(mpuReset())
        printf("Error Init mpu6050\n\r");
#else
    /* nothing to do for bma222*/
#endif
}
void sensorRead()
{
    int i;
    unsigned char ucRegOffset;
    unsigned char ucRdLen;
    volatile unsigned char temp;
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    unsigned char ucPinValue;

    // read mpu6050 or bma222 according to compilation time decision
    GPIO_IF_GetPortNPin(MOUSE_INPUT_SEL_PIN,&uiGPIOPort,&pucGPIOPin);
    ucPinValue = GPIO_IF_Get(MOUSE_INPUT_SEL_PIN,uiGPIOPort,pucGPIOPin);
    if(ucPinValue == MOUSE_INPUT_SELF_ON_VALUE){
        #ifdef USE_MPU6050
            ucRegOffset = MPU6050_RA_ACCEL_XOUT_H;
            ucRdLen = 14;
            I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&ucRegOffset,1,0);
            I2C_IF_Read(MPU6050_ADDRESS_AD0_LOW, &(aucRdDataBuf[0]), ucRdLen);
            sensorData[0] = ((short)aucRdDataBuf[ 0] << 8 | aucRdDataBuf[ 1]); // ax
            sensorData[1] = ((short)aucRdDataBuf[ 2] << 8 | aucRdDataBuf[ 3]); // ay
            sensorData[2] = ((short)aucRdDataBuf[ 4] << 8 | aucRdDataBuf[ 5]); // az
            // [6] [7] is not used (temp)
            sensorData[3] = ((short)aucRdDataBuf[ 8] << 8 | aucRdDataBuf[ 9]); // gy
            sensorData[4] = ((short)aucRdDataBuf[10] << 8 | aucRdDataBuf[11]); // gz
            sensorData[5] = ((short)aucRdDataBuf[12] << 8 | aucRdDataBuf[13]); // gx
        #else
            ucRegOffset = 0x02;
            ucRdLen = 7;
            I2C_IF_Write(BMA222_TWI_ADDR,&ucRegOffset,1,0);
            I2C_IF_Read(BMA222_TWI_ADDR, &aucRdDataBuf[0], ucRdLen);
            // only aucRdDataBuf[1], [3], [5] have data
            sensorData[0] = -aucRdDataBuf[1];
            sensorData[1] = -aucRdDataBuf[3];
            // @@ roll is not used
            for(i = 0; i < SENSOR_AXIS; i++){
                if(abs(sensorData[i] - sensorOffset[i]) <= sensorThres[i])
                    sensorData[i] = 0;
                else
                    sensorData[i] *= 1.5;
            }
        #endif
    }
    else{ // read from others
        temp = 0;
        // protocol using UART, read until 0xFF as sentinel
        while(temp != 0xFF){
            temp = (signed char)UartGetChar();
        }
        sensorData[0] = (signed char)UartGetChar();
        sensorData[1] = (signed char)UartGetChar();
        mouseClick =  (signed char)UartGetChar();
        UartPutChar((signed char)sensorData[0]);
        UartPutChar((signed char)sensorData[1]);
        UartPutChar(mouseClick);
    }

}
void sensorUpdate()
{
#ifdef USE_MPU6050
#else
    /* using bma222 */
#endif
}

/* 
    fill buff with {xmov, ymov} 
    mouseCick is not handled here
*/
void sensorToReport(char *buff)
{
#ifdef USE_MPU6050
    buff[0] = (signed char)(sensorData[0] >> 10);
    buff[1] = (signed char)(sensorData[1] >> 10);
#else
    /* using bma222 */
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    unsigned char ucPinValue;

    GPIO_IF_GetPortNPin(MOUSE_INPUT_SEL_PIN,&uiGPIOPort,&pucGPIOPin);
    ucPinValue = GPIO_IF_Get(MOUSE_INPUT_SEL_PIN,uiGPIOPort,pucGPIOPin);
    buff[0] = mouseClick;
    buff[1] = (signed char)sensorData[0];
    buff[2] = (signed char)sensorData[1];
#endif
    mouseClick = 0;
}

int mpuReset()
{
#ifdef USE_MPU6050
    /* reset device */
    // aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_1;
    // aucRdDataBuf[1] = 0x80;
    // I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],2,0);
    // vTaskDelay(pdMS_TO_TICKS(200)); // wait some ms

    /* set to correct mode */
    // cycle = 1, sleep = 0, temp_dis = 1, clksel = x_gyro
    aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_1;
    aucRdDataBuf[1] = 0x2B;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&aucRdDataBuf[0],2,0);

    // power up check
    aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_1;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&(aucRdDataBuf[0]), 1, 0);
    aucRdDataBuf[0] = 0;
    I2C_IF_Read(MPU6050_ADDRESS_AD0_LOW, &(aucRdDataBuf[0]), 1);
    printf("MPU INIT TO 0x%x\n\r", aucRdDataBuf[0]);
    // set full scale range
    // gyro
    aucRdDataBuf[0] = MPU6050_RA_GYRO_CONFIG;
    aucRdDataBuf[1] = 0x10; // +-1000 deg
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], 2, 0);
    // // accel
    aucRdDataBuf[0] = MPU6050_RA_ACCEL_CONFIG;
    aucRdDataBuf[1] = 0x00; // +-2g
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], 2, 0);
    // // set to 40Hz wake-up freq
    // aucRdDataBuf[0] = MPU6050_RA_PWR_MGMT_2;
    // aucRdDataBuf[1] = 0xC0;
    // I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], 2, 0);
    // // set sample rate to 1kHz / (1 + 4)
    aucRdDataBuf[0] = MPU6050_RA_SMPLRT_DIV;
    aucRdDataBuf[1] = 0x04;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], 2, 0);
    // // config
    aucRdDataBuf[0] = MPU6050_RA_CONFIG;
    aucRdDataBuf[1] = 0x03;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], 2, 0);
#endif
    return 0;
}

void ImuTimerIntHandler(void)
{
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static char debounceL, debounceR, debounceReset;
    unsigned int uiGPIOPort;
    unsigned char pucGPIOPin;
    unsigned char ucPinValue;

    // mouse
    GPIO_IF_GetPortNPin(MOUSE_INPUT_SEL_PIN,&uiGPIOPort,&pucGPIOPin);
    ucPinValue = GPIO_IF_Get(MOUSE_INPUT_SEL_PIN,uiGPIOPort,pucGPIOPin);
    
    // neglect all
    if(ucPinValue == MOUSE_INPUT_SELF_ON_VALUE){ // read from GPIO
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

        GPIO_IF_GetPortNPin(RESET_BTN_PIN,&uiGPIOPort,&pucGPIOPin);
        ucPinValue = GPIO_IF_Get(RESET_BTN_PIN,uiGPIOPort,pucGPIOPin);
        debounceReset = (debounceReset << 1) | ucPinValue;

        // @@ reset function is not completed
        // if(debounceReset == 0xff){
        //     sl_Stop(RESET_TIMEOUT);
        //     UartPutChar('D');
        //     Utils_TriggerHibCycle();
        // }
    }


    Timer_IF_InterruptClear(g_ulImuTimer);

}
