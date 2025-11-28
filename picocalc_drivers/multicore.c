#include "multicore.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "lcd.h"
#include "pwmsound.h"
#include "flash.h"
#include "keyboard.h"

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

void multicore_flash_start() {
	keyboard_enable_timer(false);
	multicore_reset_core1();
	multicore_launch_core1(NullCore);
	multicore_enable_irq(false);
}

void multicore_flash_end() {
	multicore_reset_core1();
	multicore_launch_core1(ResetCore);
	busy_wait_ms(250);
	multicore_reset_core1();
	multicore_enable_irq(true);
	keyboard_enable_timer(true);
}

void multicore_init() {
	multicore_fifo_drain();
	multicore_fifo_clear_irq();
	multicore_enable_irq(true);
}