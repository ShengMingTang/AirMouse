#ifndef SENSOR_H
#define SENSOR_H

#ifdef __cplusplus
extern "C"{
#endif

// this module provide an interface
// from reading sensor data, filter data
// to   output hid report

#define AX (0)
#define AY (1)
#define AZ (2)
#define GX (3)
#define GY (4)
#define GZ (5)

#ifndef USE_MPU6050
    #include "bma222.h"
    #define SENSOR_AXIS 3
    #define SENSOR_ORDER0_COV (matrix_data_t)(1.0)
    #define SENSOR_ORDER1_COV (matrix_data_t)(1.0)
    #define SENSOR_ORDER2_COV (matrix_data_t)(1.0)
    #define SENSOR_NOISE (float)(1.0)
    // arranged in [ax, ay, az, gz, gy, gz]'
#else
    #include "MPU6050.h"
    #define SENSOR_AXIS 6
#endif

void sensorInit();
void sensorRead();
void sensorUpdate();
void sensorHid(char *buff);

#define printf UART_PRINT

#ifdef __cplusplus
}
#endif

#endif
