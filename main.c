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

	set_system_mhz(240);

	fs_mount();

	char filename[256];
	while (true) {
		keyboard_flush();
		sms_file_menu("/sms", filename);
		set_system_mhz(125);
		load_rom(filename);
		set_system_mhz(240);
		sms_main();
	}
}
