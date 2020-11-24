#ifndef HID_H
#define HID_H

#ifdef __cplusplus
extern "C"{
#endif

#define SAMPLE_PERIOD_IN_MS (8) // standard is at 125Hz
// connection
#define MOUSE_PORT (5002)
#define	LISTENQ	(1024)
#define HID_REPLY_TIMEOUT (5.0)
// local
#define MAXBUFF (128)

// function interception
#define printf UART_PRINT

void hidInit();
void hidTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
