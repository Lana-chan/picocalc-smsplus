#include <stdio.h>

#include "pico/stdlib.h"

#include "lcd.h"
#include "term.h"
#include "keyboard.h"
#include "fs.h"
#include "pwmsound.h"
#include "multicore.h"

#include "sms_ui.h"

int main() {
	lcd_init();
	keyboard_init();
	stdio_picocalc_init(); 
	fs_init();
	multicore_init();
	pwmsound_init();

	fs_mount();

	ui_mainloop();
}
