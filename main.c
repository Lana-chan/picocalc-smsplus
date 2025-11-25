#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "lcd.h"
#include "term.h"
#include "keyboard.h"
#include "fs.h"
#include "pwmsound.h"
#include "multicore.h"
#include "smsplus/loadrom.h"

#include "sms_main.h"

bool set_system_mhz(uint32_t clk) {
	if (set_sys_clock_khz(clk * 1000ull, true)) {
		pwmsound_setclk();
		return true;
	}
	return false;
}

int main() {
	lcd_init();
	keyboard_init();
	stdio_picocalc_init(); 
	fs_init();
	multicore_init();
	pwmsound_init();
	load_psram_init();

	set_system_mhz(240);

	fs_mount();

	multicore_launch_core1(sms_main);

	while (true) {
		//handle_multicore_fifo();
		if (atomic_load(&fs_needs_remount) == true) {
			if (fs_mount()) {
				printf("\x1b[92mOK!\x1b[m\n");
			} else {
				printf("Failed to mount!\x1b[m\n");
			}
			atomic_store(&fs_needs_remount, false);
		}
	}
}
