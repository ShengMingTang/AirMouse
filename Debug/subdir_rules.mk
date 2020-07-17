################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="D:/cc3200/cc3200-code/AirMouse_with_cc2541" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink/" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink/include" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink/source" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/driverlib/" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/inc/" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/example/common" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/netapps" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink_extlib/provisioninglib" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/oslib" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/simplelink/ccs/OS" --include_path="C:/ti/CC3200SDK_1.5.0/cc3200-sdk/driverlib" --include_path="C:/ti/ccs930/ccs/tools/compiler/ti-cgt-arm_20.2.0.LTS/include" --define=ccs --define=__SL__ --define=USE_FREERTOS --define=SL_PLATFORM_MULTI_THREADED --define=cc3200 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


