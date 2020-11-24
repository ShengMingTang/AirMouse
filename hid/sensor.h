#ifndef SENSOR_H
#define SENSOR_H

#ifdef __cplusplus
extern "C"{
#endif

// this module provide an interface
// from reading sensor data, filter data
// to   output hid report

// device specific
#define MOUSE_INPUT_SEL_PIN (12) // mouse input source select
#define MOUSE_INPUT_SELF_ON_VALUE (0)
#define MOUSE_INPUT_OTHER_ON_VALUE (1)
#define MOUSE_BTN_LEFT_PIN (22)
#define MOUSE_BTN_RIGHT_PIN (13)

// symbolic bits
#define MOUSE_LEFT (0x04)
#define MOUSE_MID (0x02)
#define MOUSE_RIGHT (0x01)
#define KB_MAXNUM_KEY_PRESS 6 // that many key can be pressed at the same time

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
void sensorToReport(char *buff);

/* MPU6050 */
#ifdef USE_MPU6050
// return non-zero if error occurs for all fns below
int mpuReset();
#endif

#define printf UART_PRINT

#ifdef __cplusplus
}
#endif

#endif
