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
#include "sms_ui.h"

bool set_system_mhz(uint32_t clk) {
	uint32_t ints = save_and_disable_interrupts();
	if (set_sys_clock_khz(clk * 1000ull, true)) {
		pwmsound_setclk();
		restore_interrupts_from_disabled(ints);
		return true;
	}
	restore_interrupts_from_disabled(ints);
	return false;
}

int main() {
	lcd_init();
	keyboard_init();
	stdio_picocalc_init(); 
	fs_init();
	multicore_init();
	pwmsound_init();

	fs_mount();

	char filename[256];
	while (true) { // main program loop
		while(true) { // loop to menu until valid rom
			while (!ui_file_menu("/sms", filename)); // loop menu until valid file
			if (load_rom(filename)) break;
		}
		set_system_mhz(240);
		sms_main();
		set_system_mhz(125);
	}
}
