#ifndef IMU_H
#define IMU_H

#include "bma222.h"
#include "MPU6050.h"

// hardware
#define MOUSE_BTN_LEFT_PIN (22)
#define MOUSE_BTN_RIGHT_PIN (13)
// symbolic bits
#define MOUSE_LEFT (0x04)
#define MOUSE_MID (0x02)
#define MOUSE_RIGHT (0x01)
#define KB_MAXNUM_KEY_PRESS 6 // that many key can be pressed at the same time

#define SAMPLE_PERIOD_IN_MS (8) // standard is at 125Hz
#define CURSOR_VEL (500)
#define SCALE ((CURSOR_VEL)*(SAMPLE_PERIOD_IN_MS)/1000)
// coordinate index in mat
#define ACC (0)
#define GYRO (1)
#define MAG (2)
#define X (0)
#define Y (1)
#define Z (2)
// connection
#define MOUSE_PORT (5002)
#define	LISTENQ	(1024)
// local
#define MAXBUFF (128)

// function interception
#define printf UART_PRINT

void hidInit();
void hidTask(void *pvParameters);

#endif
