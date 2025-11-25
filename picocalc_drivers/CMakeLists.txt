set(LIB_NAME picocalc_drivers)

project(${LIB_NAME}
	VERSION 0.0.1
	DESCRIPTION "picocalc drivers for lcd, keyboard"
)

add_library(${LIB_NAME} INTERFACE)

pico_generate_pio_header(${LIB_NAME} ${CMAKE_CURRENT_LIST_DIR}/st7789_lcd.pio)

target_sources(${LIB_NAME} INTERFACE
	keyboard.c
	fs.c
	lcd.c
	term.c
	pwmsound.c
	multicore.c
	flash.c
)

target_link_libraries(${LIB_NAME} INTERFACE
	pico_stdlib
	hardware_spi
	hardware_pio
	hardware_i2c
	hardware_pwm
	hardware_dma
)

target_include_directories(${LIB_NAME} INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
)
