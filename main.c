// Standard includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Simplelink includes
#include "simplelink.h"
#include "netcfg.h"

//driverlib includes
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "utils.h"
#include "pin.h"
#include "uart.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"

// SD host includes
#include "sdhost.h"
#include "ff.h"

// Free_rtos/ti-rtos includes
#include "osi.h"

// common interface includes
#include "gpio_if.h"
#include "uart_if.h"
#include "i2c_if.h"
#include "common.h"

#include "smartconfig.h"
#include "pinmux.h"

//custom includes
#include "app_defines.h"
#include "app_simplelink_config.h"
#include "app_p2p.h"
#include "app_http_server.h"
#include "app_storage.h"
#include "app_ap.h"
#include "ftp/ftp_server.h"
#include "hid/hid.h"
#include "main_config.h"
// hw settings
#define SDHOST_CLK_SPEED 24000000
//*****************************************************************************
//                 EXTERN -- Start
//*****************************************************************************
extern void SDHostIntHandler();
//*****************************************************************************
//                 EXTERN -- Start
//*****************************************************************************

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long  g_ulStatus = 0;//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; //Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; //Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; //Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; //Connection BSSID
int g_iSimplelinkRole = ROLE_INVALID;
unsigned long  g_ulDeviceIp = 0;
volatile unsigned long  g_ulStaIp = 0;
unsigned char g_ucSSID[AP_SSID_LEN_MAX];
// p2p
char g_p2p_dev[MAXIMAL_SSID_LENGTH + 1];

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//*****************************************************************************
// Variable related to Connection status -- Start
//*****************************************************************************
volatile unsigned short g_usMCNetworkUstate = 0;
int g_uiSimplelinkRole = ROLE_INVALID;
unsigned int g_uiDeviceModeConfig = ROLE_STA; //default is STA mode
volatile unsigned char g_ucConnectTimeout =0;
//*****************************************************************************
// Variable related to Connection status -- End
//*****************************************************************************

void main(void)
{
    long lRetVal = -1;

    //*****************************************************************************
    //              Board Routines -- Start
    //*****************************************************************************

    //Board Initialization
    BoardInit();

    //Pin Configuration
    PinMuxConfig();

    //Change Pin 58 Configuration from Default to Pull Down
    MAP_PinConfigSet(PIN_58,PIN_STRENGTH_2MA|PIN_STRENGTH_4MA,PIN_TYPE_STD_PD);

    //
    // Initialize GREEN and ORANGE LED
    //
    GPIO_IF_LedConfigure(LED1|LED2|LED3);
    //Turn Off the LEDs
    GPIO_IF_LedOff(MCU_ALL_LED_IND);

    //UART Initialization
    MAP_PRCMPeripheralReset(PRCM_UARTA0);

    MAP_UARTConfigSetExpClk(CONSOLE,MAP_PRCMPeripheralClockGet(CONSOLE_PERIPH),
                            UART_BAUD_RATE,(UART_CONFIG_WLEN_8 |
                                UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    //Display Application Banner on UART Terminal
    DisplayBanner(APPLICATION_NAME);

    //*****************************************************************************
    //              Board Routines -- End
    //*****************************************************************************
    
    //*****************************************************************************
    //              FatFs -- Start
    //*****************************************************************************

    // @@udma
    // Initialize Udma
    //
    UDMAInit();

    //
    // Set the SD card clock as output pin
    //
    MAP_PinDirModeSet(PIN_07,PIN_DIR_MODE_OUT);

    //
    // Enable Pull up on data
    //
    MAP_PinConfigSet(PIN_06,PIN_STRENGTH_4MA, PIN_TYPE_STD_PU);

    //
    // Enable Pull up on CMD
    //
    MAP_PinConfigSet(PIN_08,PIN_STRENGTH_4MA, PIN_TYPE_STD_PU);

    // Enable MMCHS
    //
    MAP_PRCMPeripheralClkEnable(PRCM_SDHOST,PRCM_RUN_MODE_CLK);

    //
    // Reset MMCHS
    //
    MAP_PRCMPeripheralReset(PRCM_SDHOST);

    //
    // Configure MMCHS
    //
    MAP_SDHostInit(SDHOST_BASE);

    //
    // Configure card clock
    //
    MAP_SDHostSetExpClk(SDHOST_BASE,
                            MAP_PRCMPeripheralClockGet(PRCM_SDHOST),SDHOST_CLK_SPEED);

    // @@udma
    //
    // Register Interrupt
    //
    MAP_SDHostIntRegister(SDHOST_BASE, SDHostIntHandler);

    // @@udma
    //
    // Interrupt Enable
    //
    MAP_SDHostIntEnable(SDHOST_BASE, SDHOST_INT_DMARD | SDHOST_INT_DMAWR);

    //*****************************************************************************
    //              FatFs -- End
    //*****************************************************************************

    //*****************************************************************************
    //              Tasks -- Start
    //*****************************************************************************
    UART_PRINT("Start creating tasks\n\r");

    //
    // Simplelinkspawntask
    //
    lRetVal = VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
    
    // //
    // // Create p2p(connection) management Task
#if defined(USE_P2P)
    #warning "P2P is used"
    lRetVal = osi_TaskCreate(P2PManagerTask, (signed char*)"P2PManagerTask",
        OSI_STACK_SIZE, NULL, OOB_TASK_PRIORITY, NULL
    );
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#endif
#if defined(USE_AP)
    #warning "AP is used"
    lRetVal = osi_TaskCreate(APTask, (signed char*)"APTask",
        OSI_STACK_SIZE, NULL, OOB_TASK_PRIORITY, NULL
    );
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#endif
#if defined(USE_FTP)
    #warning "FTP is used"
    ftpServerInit();
    lRetVal = osi_TaskCreate(ftpServerTask, (signed char*)"FtpTask",
        OSI_STACK_SIZE, NULL, OOB_TASK_PRIORITY, NULL
    );
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#endif

    /* hid */
#if defined(USE_HID)
    #warning "HID is used"
    hidInit();
    lRetVal = osi_TaskCreate(hidTask, (signed char*)"hidTask",
        OSI_STACK_SIZE, NULL, 8, NULL
    );
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#endif

    //
    // Create HTTP Server Task
#if defined(USE_HTTP)
    #warning "HTTP is used"
    lRetVal = osi_TaskCreate(HTTPServerTask, (signed char*)"httpServer",
        OSI_STACK_SIZE, NULL, OOB_TASK_PRIORITY, NULL
    );
    if(lRetVal < 0){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#endif

    //
    // Init and Create Storage Tasks
    //*****************************************************************************
    //              Tasks -- End
    //*****************************************************************************

    //
    // Start OS Scheduler
    //
    osi_start();

    while (1)
    {

    }
}
