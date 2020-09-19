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
#include "app_simplelink_config.h"
#include "app_p2p.h"
#include "app_ap.h"
#include "ftp/ftp_server.h"
#include "hid/hid.h"

// hw settings
#define SDHOST_CLK_SPEED 24000000
extern void SDHostIntHandler();

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
    
#ifndef USE_AP
    // //
    // // Create p2p(connection) management Task
    #warning "P2P is used"
    lRetVal = osi_TaskCreate(P2PManagerTask, (signed char*)"P2PManagerTask",
        OSI_STACK_SIZE, NULL, 8, NULL
    );
    if(lRetVal != OSI_OK){
        ERR_PRINT(lRetVal);
        LOOP_FOREVER();
    }
#else
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
        OSI_STACK_SIZE, NULL, 8, NULL
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
