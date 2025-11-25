#include "multicore.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "lcd.h"
#include "pwmsound.h"
#include "flash.h"
#include "keyboard.h"

volatile atomic_bool multicore_flash_in_progress = false;
uint32_t ints;

#define FLASH_ERASE 1
#define FLASH_PROGRAM 2

int flash_operation = 0;
uint32_t flash_address;
const void* flash_data;
uint32_t flash_size;

void handle_multicore_fifo() {
	// take first FIFO packet and pass to different handlers
	while (multicore_fifo_rvalid()) {
		uint32_t packet = multicore_fifo_pop_blocking_inline();
		
		if (lcd_fifo_receiver(packet)) break;
		if (pwmsound_fifo_receiver(packet)) break;
	}

	multicore_fifo_clear_irq();
}

void multicore_fifo_push_string(const char* source, size_t len) {
	char* dest = strndup(source, len);
	if (!dest) {
		multicore_fifo_push_blocking_inline(0);
		multicore_fifo_push_blocking_inline((uint32_t)NULL);
	}
	multicore_fifo_push_blocking_inline(len);
	multicore_fifo_push_blocking_inline((uint32_t)dest);
}

size_t multicore_fifo_pop_string(char** string) {
	size_t len = multicore_fifo_pop_blocking_inline();
	*string = (char*)multicore_fifo_pop_blocking_inline();
	return len;
}

void multicore_enable_irq(bool enable) {
	if (enable) irq_set_exclusive_handler(SIO_FIFO_IRQ_NUM(0), handle_multicore_fifo);
	else irq_remove_handler(SIO_FIFO_IRQ_NUM(0), handle_multicore_fifo);
	irq_set_enabled(SIO_FIFO_IRQ_NUM(0), enable);
}

static void inline multicore_flash_start() {
	multicore_enable_irq(false);
	keyboard_enable_timer(false);
}

static void inline multicore_flash_end() {
	keyboard_enable_timer(true);
	//multicore_enable_irq(true);
}

void multicore_check_and_perform_flash() {
	if (atomic_load(&multicore_flash_in_progress) == false) return;

	multicore_flash_start();

	if (flash_operation == FLASH_ERASE) {
		flash_erase(flash_address, flash_size);
	} else if (flash_operation == FLASH_PROGRAM) {
		flash_program(flash_address, flash_data, flash_size);
	}
	//flash_flush_cache();
	flash_operation = 0;

	atomic_store(&multicore_flash_in_progress, false);
	
	multicore_flash_end();
}

void multicore_init() {
	multicore_fifo_drain();
	multicore_fifo_clear_irq();
	//multicore_enable_irq(true);
}

void multicore_flash_erase(uint32_t address, uint32_t size_bytes) {
	flash_operation = FLASH_ERASE;
	flash_address = address;
	flash_size = size_bytes;
	atomic_store(&multicore_flash_in_progress, true);
	multicore_wait_flash_blocking();
}

void multicore_flash_program(uint32_t address, const void* buf, uint32_t size_bytes) {
	flash_operation = FLASH_PROGRAM;
	flash_address = address;
	flash_data = buf;
	flash_size = size_bytes;
	atomic_store(&multicore_flash_in_progress, true);
	multicore_wait_flash_blocking();
}