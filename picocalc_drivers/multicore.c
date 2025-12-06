#include "multicore.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "lcd.h"
#include "pwmsound.h"
#include "flash.h"
#include "keyboard.h"

uint32_t ints;

#define MULTICORE_TIMER_US 1000
repeating_timer_t multicore_queue_timer;

#define QUEUE_SIZE 32
uint32_t multicore_queue[QUEUE_SIZE];
int multicore_queue_head, multicore_queue_tail; 

static bool multicore_queue_empty() {
	return multicore_queue_head == multicore_queue_tail;
}

static bool multicore_queue_full() {
	return (multicore_queue_head+1) % QUEUE_SIZE == multicore_queue_tail;
}

void mutlicore_queue_push(uint32_t data) {
	while (multicore_queue_full()) tight_loop_contents();
	multicore_queue[multicore_queue_head] = data;
	multicore_queue_head = (multicore_queue_head + 1) % QUEUE_SIZE;
}

uint32_t multicore_queue_pop() {
	while (multicore_queue_empty()) tight_loop_contents();
	uint32_t data = multicore_queue[multicore_queue_tail];
	multicore_queue_tail = (multicore_queue_tail + 1) % QUEUE_SIZE;
	return data;
}

void handle_multicore_fifo() {
	// take first FIFO packet and pass to different handlers
	while (!multicore_queue_empty()) {
		uint32_t packet = multicore_queue_pop();
		
		if (lcd_fifo_receiver(packet)) break;
		if (pwmsound_fifo_receiver(packet)) break;
	}
}

bool multicore_queue_timer_callback(repeating_timer_t *rt) {
	handle_multicore_fifo();
	return true;
}

void multicore_fifo_push_string(const char* source, size_t len) {
	char* dest = strndup(source, len);
	if (!dest) {
		mutlicore_queue_push(0);
		mutlicore_queue_push((uint32_t)NULL);
	}
	mutlicore_queue_push(len);
	mutlicore_queue_push((uint32_t)dest);
}

size_t multicore_fifo_pop_string(char** string) {
	size_t len = multicore_queue_pop();
	*string = (char*)multicore_queue_pop();
	return len;
}

void multicore_enable_timer(bool enable) {
	if (enable) add_repeating_timer_us(MULTICORE_TIMER_US, multicore_queue_timer_callback, NULL, &multicore_queue_timer);
	else cancel_repeating_timer(&multicore_queue_timer);
}

void multicore_flash_start() {
	keyboard_enable_timer(false);
	multicore_enable_timer(false);
	ints = save_and_disable_interrupts();
}

void multicore_flash_end() {
	restore_interrupts_from_disabled(ints);	
	multicore_enable_timer(true);
	keyboard_enable_timer(true);
}

void multicore_init() {
	multicore_queue_head = 0;
	multicore_queue_tail = 0;
	multicore_enable_timer(true);
}