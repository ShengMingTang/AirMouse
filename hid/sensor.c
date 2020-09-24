// Standard includes
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

// Kalman includes
#include "sensor.h"
#include "hid.h"
// create the filter structure
#define KALMAN_NAME hid
#define KALMAN_NUM_STATES 3
#define KALMAN_NUM_INPUTS 0
#include "Kalman/kalman_factory_filter.h"
// create the measurement structure
#define KALMAN_MEASUREMENT_NAME position
#define KALMAN_NUM_MEASUREMENTS 3 // was 1
#include "Kalman/kalman_factory_measurement.h"
// clean up
#include "Kalman/kalman_factory_cleanup.h"

static kalman_t *kf[SENSOR_AXIS];
static kalman_measurement_t *kfm[SENSOR_AXIS];
#ifndef USE_MPU6050
static const matrix_data_t initState[SENSOR_AXIS] = {100.0, 100.0, 9.8*1000}; // az
static const int sensorOffset[SENSOR_AXIS] = {0, 0, 67};
static const int sensorThres[SENSOR_AXIS] = {5, 3, 5};
#else
#endif
const matrix_data_t T = 1; // set time constant
static signed char  aucRdDataBuf[32];
static signed short sensorData[SENSOR_AXIS];

void sensorInit()
{
    matrix_t *x; // state
    matrix_t *A; // state transition matrix
    matrix_t *P; // covariance matrix
    matrix_t *H; // measurement matrix
    matrix_t *R; // process noise
    int i;
    /************************************************************************/
    /* initialize the filter structures                                     */
    /************************************************************************/
    for(i = 0; i < SENSOR_AXIS; i++){
        kf[i] = kalman_filter_hid_init();
        kfm[i] = kalman_filter_hid_measurement_position_init();
        /* set initial state */
        x = kalman_get_state_vector(kf[i]);
        x->data[0] = 0;
        x->data[1] = 0;
        x->data[2] = initState[i];
        /* set state transition */
        A = kalman_get_state_transition(kf[i]);
        // x0 (s, postion), x1 (v, velocity), x2 (g, acceleration)
        // transition of x to x0 = x0 + T*v + 1/2*g*T^2
        matrix_set(A, 0, 0, 1);   // 1
        matrix_set(A, 0, 1, T);   // T
        matrix_set(A, 0, 2, (matrix_data_t)0.5*T*T); // 0.5 * T^2
        // transition of x to x1 = x1 + T*x2
        matrix_set(A, 1, 0, 0);   // 0
        matrix_set(A, 1, 1, 1);   // 1
        matrix_set(A, 1, 2, T);   // T
        // transition of x to x2 = x2
        matrix_set(A, 2, 0, 0);   // 0
        matrix_set(A, 2, 1, 0);   // 0
        matrix_set(A, 2, 2, 1);   // 1
        /* set covariance */
        P = kalman_get_system_covariance(kf[i]);
        matrix_set_symmetric(P, 0, 0, SENSOR_ORDER0_COV);   // var(s)
        matrix_set_symmetric(P, 0, 1, 0);   // cov(s,v)
        matrix_set_symmetric(P, 0, 2, 0);   // cov(s,g)
        matrix_set_symmetric(P, 1, 1, SENSOR_ORDER1_COV);   // var(v)
        matrix_set_symmetric(P, 1, 2, 0);   // cov(v,g)
        matrix_set_symmetric(P, 2, 2, SENSOR_ORDER2_COV);   // var(g)
        /* set measurement transformation */
        H = kalman_get_measurement_transformation(kfm[i]);
        matrix_set(H, 0, 0, 0);     // was z = 1*s 
        matrix_set(H, 0, 1, 1);     //       + 0*v
        matrix_set(H, 0, 2, 0);     //       + 0*g
        /* set process noise */
        R = kalman_get_process_noise(kfm[i]);
        matrix_set(R, 0, 0, (matrix_data_t)(SENSOR_NOISE));     // var(s)
    }

    /* hardware init */
#ifndef USE_MPU6050
    /* nothing to do for bma222*/
#else
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
#endif
}
void sensorRead()
{
    unsigned char ucRegOffset;
    unsigned char ucRdLen;
    int i;
#ifndef USE_MPU6050
    ucRegOffset = 0x02;
    ucRdLen = 7;
    I2C_IF_Write(BMA222_TWI_ADDR,&ucRegOffset,1,0);
    I2C_IF_Read(BMA222_TWI_ADDR, &aucRdDataBuf[0], ucRdLen);
    // only aucRdDataBuf[1], [3], [5] have data
    sensorData[0] = aucRdDataBuf[1];
    sensorData[1] = aucRdDataBuf[3];
    sensorData[2] = aucRdDataBuf[5];
    for(i = 0; i < SENSOR_AXIS; i++){
        if(abs(sensorData[i] - sensorOffset[i]) <= sensorThres[i])
            sensorData[i] = 0;
        // else
        //     sensorData[i] *= 1.5;
    }
#else
    ucRegOffset = MPU6050_RA_ACCEL_XOUT_H;
    ucRdLen = 14;
    I2C_IF_Write(MPU6050_ADDRESS_AD0_LOW,&ucRegOffset,1,0);
    I2C_IF_Read(MPU6050_ADDRESS_AD0_LOW, &aucRdDataBuf[0], ucRdLen);
    int a, b, c;
    sensorData[0] = ((int)aucRdDataBuf[ 0] << 8 | aucRdDataBuf[ 1]); // ax
    sensorData[1] = ((int)aucRdDataBuf[ 2] << 8 | aucRdDataBuf[ 3]); // ay
    sensorData[2] = ((int)aucRdDataBuf[ 4] << 8 | aucRdDataBuf[ 5]); // az
    // [6] [7] is not used (temp)
    sensorData[4] = ((int)aucRdDataBuf[ 8] << 8 | aucRdDataBuf[ 9]); // gy
    sensorData[5] = ((int)aucRdDataBuf[10] << 8 | aucRdDataBuf[11]); // gz
    sensorData[6] = ((int)aucRdDataBuf[12] << 8 | aucRdDataBuf[13]); // gx
#endif
}
void sensorUpdate()
{
    matrix_t *x; // state
    matrix_t *z; // measure matrix
    matrix_data_t measurement; // measurement
    int i;
#ifndef USE_MPU6050
    for(i = 0; i < SENSOR_AXIS; i++){
        x = kalman_get_state_vector(kf[i]);
        z = kalman_get_measurement_vector(kfm[i]);
        // predict
        kalman_predict(kf[i]);
        // measure
        measurement = (matrix_data_t)(sensorData[i]) * 9.8 * 1000 / 64 * T + x->data[1];
        matrix_set(z, 0, 1, measurement); // was 0,0,measurement
        // update
        kalman_correct(kf[i], kfm[i]);
    }
#else
#endif
}

/* 
    fill buff with {xmov, ymov} 
    mouseCick is not handled here
*/
void sensorHid(char *buff)
{
    matrix_t *x, *y; // state
#ifndef USE_MPU6050
    x = kalman_get_state_vector(kf[0]);
    y = kalman_get_state_vector(kf[1]);
    buff[0] = -(char)(sensorData[0]);
    buff[1] = -(char)(sensorData[1]);
    // keyboard will fill in later
#else
#endif
}