#ifndef APP_SIMPLELINK_CONFIG_H_
#define APP_SIMPLELINK_CONFIG_H_

#ifdef __cplusplus
extern "C"{
#endif

#define APPLICATION_NAME        "Air Mouse"
#define APPLICATION_VERSION     "1.4.0"

#define AP_SSID_LEN_MAX         (33)
#define ROLE_INVALID            (-5)
#define OOB_TASK_PRIORITY               (1)
#define OSI_STACK_SIZE                  (1600) // was 1024
#define SH_GPIO_3                       (3)  /* P58 - Device Mode */
#define ROLE_INVALID                    (-5)
#define AUTO_CONNECTION_TIMEOUT_COUNT   (50)   /* 5 Sec */

typedef enum{
    // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
    LAN_CONNECTION_FAILED = -0x7D0,
    INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
    // DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

    // Choosing this number to avoid overlap w/ host-driver's error codes
    NETWORK_CONNECTION_FAILED = -0x7D0,
    P2P_CONFIG_FAILED = NETWORK_CONNECTION_FAILED - 1,
    P2P_MODE_START_FAILED = P2P_CONFIG_FAILED - 1,
    DEVICE_NOT_IN_STATION_MODE = P2P_MODE_START_FAILED - 1,
    CLIENT_DISCONNECTED = DEVICE_NOT_IN_STATION_MODE -1,

   STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

void DisplayBanner(char * AppName);
void BoardInit(void);
void InitializeAppVariables();
long ConfigureSimpleLinkToDefaultState();
void ReadDeviceConfiguration();

signed long DisplayIP();

#ifdef __cplusplus
}
#endif

#endif /* APP_SIMPLELINK_CONFIG_H_ */
