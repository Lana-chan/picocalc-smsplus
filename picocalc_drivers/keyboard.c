#include <stdio.h>
#include <stdatomic.h>

#include <pico/stdio.h>
#include "pico/util/queue.h"
#include <hardware/gpio.h>
#include <hardware/i2c.h>

#include "keyboard.h"

#define KBD_MOD    i2c1
#define KBD_SDA    6
#define KBD_SCL    7
#define KBD_SPEED  10000 // if dual i2c, then the speed of keyboard i2c should be 10khz
#define KBD_ADDR   0x1F

// Commands defined by the keyboard driver
enum {
	REG_ID_VER = 0x01,     // fw version
	REG_ID_CFG = 0x02,     // config
	REG_ID_INT = 0x03,     // interrupt status
	REG_ID_KEY = 0x04,     // key status
	REG_ID_BKL = 0x05,     // backlight
	REG_ID_DEB = 0x06,     // debounce cfg
	REG_ID_FRQ = 0x07,     // poll freq cfg
	REG_ID_RST = 0x08,     // reset
	REG_ID_FIF = 0x09,     // fifo
	REG_ID_BK2 = 0x0A,     //keyboard backlight
	REG_ID_BAT = 0x0b,     // battery
	REG_ID_C64_MTX = 0x0c, // read c64 matrix
	REG_ID_C64_JS = 0x0d,  // joystick io bits
};

#define I2C_TIMEOUT 15000
volatile atomic_bool i2c_in_use = false;

#define KEY_COUNT 256
unsigned char keyboard_states[KEY_COUNT];

// timer and circular buffer and other ideas taken and modified from BlairLeduc's picocalc-text-starter
// https://github.com/BlairLeduc/picocalc-text-starter/blob/main/drivers/keyboard.c
#define KBD_BUFFER_SIZE 32
static volatile input_event_t rx_buffer[KBD_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static repeating_timer_t key_timer;
queue_t key_fifo;

keyboard_callback_t key_available_callback = NULL;
keyboard_callback_t interrupt_callback = NULL;

static int keyboard_modifiers;

bool queue_enabled;

static int i2c_kbd_write(unsigned char* data, int size) {
	if (atomic_load(&i2c_in_use) == true) return 0;
	atomic_store(&i2c_in_use, true);
	int retval = i2c_write_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, I2C_TIMEOUT);
	atomic_store(&i2c_in_use, false);
	if (retval == PICO_ERROR_GENERIC) {
		printf("i2c_kbd_write: i2c write error\n");
		return 0;
	}
	return 1;
}

static int i2c_kbd_read(unsigned char* data, int size) {
	if (atomic_load(&i2c_in_use) == true) return 0;
	atomic_store(&i2c_in_use, true);
	int retval = i2c_read_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, I2C_TIMEOUT);
	atomic_store(&i2c_in_use, false);
	if (retval == PICO_ERROR_GENERIC) {
		printf("i2c_kbd_read: i2c read error\n");
		return 0;
	}
	return 1;
}

static int i2c_kbd_command(unsigned char command) {
	return i2c_kbd_write(&command, 1);
}

static int i2c_kbd_queue_size() {
	if (!i2c_kbd_command(REG_ID_KEY)) return 0; // Read queue size 
	unsigned short result = 0;
	if (!i2c_kbd_read((unsigned char*)&result, 2)) return 0;
	return result & 0x1f; // bits beyond that mean something different
}

static unsigned short i2c_kbd_read_key() {
	if (!i2c_kbd_command(REG_ID_FIF)) return KEY_NONE;
	unsigned short result = 0;
	if (!i2c_kbd_read((unsigned char*)&result, 2)) return KEY_NONE;
	return result;
}

static void update_modifiers(unsigned short value) {
	switch (value) {
		case (KEY_CONTROL << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_CONTROL; break;
		case (KEY_CONTROL << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_CONTROL; break;
		case (KEY_ALT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_ALT; break;
		case (KEY_ALT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_ALT; break;
		case (KEY_LSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_LSHIFT; break;
		case (KEY_LSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_LSHIFT; break;
		case (KEY_RSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_RSHIFT; break;
		case (KEY_RSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_RSHIFT; break;
	}
}

#include "pico/bootrom.h"
#include "hardware/watchdog.h"

static bool on_keyboard_timer(repeating_timer_t *rt) {
	int queue_size = i2c_kbd_queue_size();
	while(queue_size) { // we want to dequeue the whole i2c fifo
		unsigned short value = i2c_kbd_read_key();
		if (value == KEY_NONE) continue; // didn't get a i2c read, don't update anything
		char state = value & 0xff;
		char code = value >> 8;
		if (state == KEY_STATE_PRESSED) {
			keyboard_states[code] = KEY_STATE_PRESSED;
		} else if (state == KEY_STATE_RELEASED) {
			keyboard_states[code] = KEY_STATE_IDLE;
		}
		//update_modifiers(value);
		if (queue_enabled) {
			input_event_t event = {state, keyboard_modifiers, code};
			queue_try_add(&key_fifo, &event);
		}
		queue_size--;
		//if (key_available_callback) key_available_callback();
	}
	return true;
}

void keyboard_set_key_available_callback(keyboard_callback_t callback) {
	key_available_callback = callback;
}

void keyboard_set_interrupt_callback(keyboard_callback_t callback) {
	interrupt_callback = callback;
}

bool keyboard_key_available() {
	return !queue_is_empty(&key_fifo);
}

void keyboard_flush() {
	while (queue_try_remove(&key_fifo, NULL)) tight_loop_contents();
}

unsigned char keyboard_getstate(unsigned char code) {
	return keyboard_states[code];
}

input_event_t keyboard_poll(bool peek) {
	if (!keyboard_key_available()) return (input_event_t){0, 0, KEY_NONE};
	input_event_t event;
	if (peek) queue_peek_blocking(&key_fifo, &event);
	else queue_remove_blocking(&key_fifo, &event);
	return event;
}

input_event_t keyboard_wait_ex(bool nomod, bool onlypressed) {
	input_event_t event;
	while(true) { 
		while (!keyboard_key_available())	tight_loop_contents();

		event = keyboard_poll(false);
		if (!onlypressed || event.state == KEY_STATE_PRESSED) {
			if (event.code != KEY_NONE) {
				if (!nomod) { // only returns if it's not a modifier
					break;
				} else {
					if (event.code != KEY_CONTROL ||
						event.code != KEY_ALT ||
						event.code != KEY_LSHIFT ||
						event.code != KEY_RSHIFT) break;
				}
			}
		}
	}
	return event;
}

int keyboard_init() {
	i2c_init(KBD_MOD, KBD_SPEED);
	gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
	gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
	gpio_pull_up(KBD_SCL);
	gpio_pull_up(KBD_SDA);
	keyboard_modifiers = 0;
	memset(keyboard_states, KEY_STATE_IDLE, KEY_COUNT);
	queue_init(&key_fifo, sizeof(input_event_t), KBD_BUFFER_SIZE);
	keyboard_enable_queue(true);
	while (i2c_kbd_read_key() != 0); // Drain queue
	keyboard_enable_timer(true);
}

void keyboard_enable_timer(bool enable) {
	if (enable) {
		add_repeating_timer_ms(-20, on_keyboard_timer, NULL, &key_timer);
	} else {
		cancel_repeating_timer(&key_timer);
	}

}

void keyboard_enable_queue(bool enable) {
	if (enable) {
		while (!queue_is_empty(&key_fifo)) queue_remove_blocking(&key_fifo, NULL);
	}
	queue_enabled = enable;
}

int get_battery(bool* charging) {
	if (!i2c_kbd_command(REG_ID_BAT)) return -1;
	int result = 0;
	if (!i2c_kbd_read((unsigned char*)&result, 2)) return -1;
	result >>= 8;
	if (charging) *charging = (result & 1<<7) != 0;
	return result & ~(1<<7);
}