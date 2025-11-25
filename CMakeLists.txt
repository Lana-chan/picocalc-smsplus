# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
set(USERHOME $ENV{USERPROFILE})
else()
set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picocalc-smsplus C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(pico_fatfs)
add_subdirectory(picocalc_drivers)
add_subdirectory(smsplus)
add_subdirectory(rp2040-psram)

# Add executable. Default name is the project name, version 0.1

add_executable(picocalc-smsplus
	main.c
	sms_main.c
)

pico_set_program_name(picocalc-smsplus "picocalc-smsplus")
pico_set_program_version(picocalc-smsplus "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(picocalc-smsplus 0)
pico_enable_stdio_usb(picocalc-smsplus 0)

# Add the standard library to the build
target_link_libraries(picocalc-smsplus
	pico_stdlib
	pico_fatfs
	picocalc_drivers
	pico_multicore
	smsplus
	rp2040-psram
)

# Add the standard include files to the build
target_include_directories(picocalc-smsplus PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(picocalc-smsplus 
	hardware_spi
	hardware_i2c
	hardware_pio
	hardware_timer
	hardware_clocks
	hardware_flash
)

target_compile_definitions(picocalc-smsplus PRIVATE
	# rp2040-psram
	PSRAM_SPINLOCK=1
	PSRAM_ASYNC=1
	PSRAM_PIN_CS=20
	PSRAM_PIN_SCK=21
	PSRAM_PIN_MOSI=2
	PSRAM_PIN_MISO=3
)

pico_add_extra_outputs(picocalc-smsplus)

