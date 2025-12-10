#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "pico/multicore.h"
#include "pico/flash.h"

enum FIFO_CODES {
	FIFO_LCD = UINT32_MAX - 100,
	FIFO_LCD_POINT,
	FIFO_LCD_DRAW,
	FIFO_LCD_PALDRAW,
	FIFO_LCD_FILL,
	FIFO_LCD_CLEAR,
	FIFO_LCD_BUFEN,
	FIFO_LCD_BUFBLIT,
	FIFO_LCD_CHAR,
	FIFO_LCD_TEXT,
	FIFO_LCD_SCROLL,
	FIFO_LCD_WAIT,

	FIFO_PWM,
	FIFO_PWM_FILLBUF,
	FIFO_PWM_CLEARBUF,
	FIFO_PWM_ENABLEDMA,
};

extern volatile atomic_bool multicore_flash_in_progress;

bool set_system_mhz(uint32_t clk);

void multicore_fifo_push_string(const char* string, size_t len);
size_t multicore_fifo_pop_string(char** string);

void multicore_init();
void handle_multicore_fifo();
void multicore_queue_push(uint32_t data);
uint32_t multicore_queue_pop();

void multicore_flash_start();
void multicore_flash_end();