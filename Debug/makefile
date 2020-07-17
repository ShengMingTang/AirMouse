################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./app_freertos_handler.obj" \
"./app_http_server.obj" \
"./app_p2p.obj" \
"./app_simpleLink_handlers.obj" \
"./app_simplelink_config.obj" \
"./app_storage.obj" \
"./gpio_if.obj" \
"./main.obj" \
"./network_common.obj" \
"./pinmux.obj" \
"./smart_config.obj" \
"./startup_ccs.obj" \
"./uart_if.obj" \
"../cc3200v1p32.cmd" \
$(GEN_CMDS__FLAG) \
-l"C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink/ccs/OS_debug/simplelink.a" \
-l"C:/ti/CC3200SDK_1.5.0/cc3200-sdk/driverlib/ccs/Release/driverlib.a" \
-l"C:/ti/CC3200SDK_1.5.0/cc3200-sdk/oslib/ccs/free_rtos/free_rtos.a" \
-l"C:/ti/CC3200SDK_1.5.0/cc3200-sdk/netapps/http/client/ccs/HTTPClientMinLib/webclient.a" \
-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
AirMouse_with_cc2541.out \

EXE_OUTPUTS__QUOTED += \
"AirMouse_with_cc2541.out" \

BIN_OUTPUTS += \
AirMouse_with_cc2541.hex \

BIN_OUTPUTS__QUOTED += \
"AirMouse_with_cc2541.hex" \


# All Target
all: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "AirMouse_with_cc2541.out"

# Tool invocations
AirMouse_with_cc2541.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: ARM Linker'
	"C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --define=ccs --define=__SL__ --define=USE_FREERTOS --define=SL_PLATFORM_MULTI_THREADED --define=cc3200 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi -z -m"AirMouse_with_cc2541.map" --heap_size=0x8000 --stack_size=0x800 -i"C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/lib" -i"C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/include" --reread_libs --diag_wrap=off --display_error_number --warn_sections --xml_link_info="AirMouse_with_cc2541_linkInfo.xml" --rom_model -o "AirMouse_with_cc2541.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

AirMouse_with_cc2541.hex: $(EXE_OUTPUTS)
	@echo 'Building files: $(strip $(EXE_OUTPUTS__QUOTED))'
	@echo 'Invoking: ARM Hex Utility'
	"C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/bin/armhex"  -o "AirMouse_with_cc2541.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building: $(strip $(EXE_OUTPUTS__QUOTED))'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(BIN_OUTPUTS__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) "app_freertos_handler.obj" "app_http_server.obj" "app_p2p.obj" "app_simpleLink_handlers.obj" "app_simplelink_config.obj" "app_storage.obj" "gpio_if.obj" "main.obj" "network_common.obj" "pinmux.obj" "smart_config.obj" "startup_ccs.obj" "uart_if.obj" 
	-$(RM) "app_freertos_handler.d" "app_http_server.d" "app_p2p.d" "app_simpleLink_handlers.d" "app_simplelink_config.d" "app_storage.d" "gpio_if.d" "main.d" "network_common.d" "pinmux.d" "smart_config.d" "startup_ccs.d" "uart_if.d" 
	-@echo 'Finished clean'
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

