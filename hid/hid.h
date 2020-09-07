#ifndef IMU_H
#define IMU_H

#define SAMPLE_PERIOD_IN_MS 100

#define ACC (0)
#define GYRO (1)
#define MAG (2)

#define X (0)
#define Y (1)
#define Z (2)

#define RECVR_PORT 5002

#define printf UART_PRINT

#define MAXBUFF 128

void hidInit();
void hidTask(void *pvParameters);

#endif
