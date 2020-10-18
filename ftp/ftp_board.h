#ifndef FTP_BOARD_INCLUDE_H
#define FTP_BOARD_INCLUDE_H

#ifdef __cplusplus
extern "C"{
#endif
// simplelink includes 
#include "simplelink.h"
#include "wlan.h"

// driverlib includes 
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "uart.h"
#include "utils.h"
#include "hw_mmchs.h" // @@udma
#include "udma.h" // @@udma
#include "sdhost.h"

// common interface includes 
#include "udma_if.h"
#include "common.h"
#include "uart_if.h"

#include "pinmux.h"

// function interface for routing I/O
#warning "printf() is intercepted to UART_PRINT()"
#define printf      UART_PRINT

#ifdef __cplusplus
}
#endif

#endif
