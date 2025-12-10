#include "sms_ui.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "picocalc_drivers/multicore.h"
#include "picocalc_drivers/keyboard.h"
#include "picocalc_drivers/term.h"
#include "picocalc_drivers/fs.h"
#include "pico/time.h"

#include "smsplus/loadrom.h"
#include "smsplus/state.h"
#include "sms_main.h"

extern const char* GIT_DESC;

#define UI_STATUS_SIZE 128
#define UI_STATUS_CLEAR_MS 5000
char ui_status[UI_STATUS_SIZE];
bool ui_status_updated;
alarm_id_t ui_status_alarm;

#define MAX_PATH 256
char filename[MAX_PATH];

bool is_rom(const char* filename) {
	if (strcasecmp(strrchr(filename, '.'), ".sms") == 0) return true;
	if (strcasecmp(strrchr(filename, '.'), ".gg") == 0) return true;
	if (strcasecmp(strrchr(filename, '.'), ".sg") == 0) return true;
	return false;
}

void center_print(int row, const char* str) {
	int column = (term_get_width()-strlen(str))/2 + 1;
	printf("\x1b[%d;%dH%s", row, column, str);
}

int64_t ui_status_clear(alarm_id_t id, void *user_data) {
	ui_status[0] = '\0';
	return 0;
}

void ui_status_set(const char* fmt, ...) {
	va_list list;
	va_start(list, fmt);
	vsnprintf(ui_status, UI_STATUS_SIZE, fmt, list);
	ui_status_alarm = add_alarm_in_ms(UI_STATUS_CLEAR_MS, ui_status_clear, NULL, true);
	ui_status_updated = 1;
}

void ui_status_print() {
	int width = term_get_width()-5;
	int row = term_get_height();
	int battery = get_battery(NULL);
	printf("\x1b[%d;1H%-*.*s %d%%", row, width, width, ui_status, battery);
	ui_status_updated = 0;
}

void ui_handle_savestate(int slot, int save) {
	FIL fp;
	FRESULT res;

	keyboard_states['0' + slot] = KEY_STATE_IDLE;
	
	char savename[MAX_PATH];
	int delim = strcspn(filename, ".");
	sprintf(savename, "%.*s.%d.sav", delim, filename, slot);	
	
	if (save) {
		// TODO: `filename` shouldn't contain path
		/*if (!fs_exists("/sms/saves")) {
			f_mkdir("/sms/saves");
		}*/

		res = f_open(&fp, savename, FA_CREATE_ALWAYS | FA_WRITE);
		if (res == FR_OK) {
			system_save_state(&fp);
		} else {
			ui_status_set("Could not save state %d", slot);
		}
	} else {
		res = f_open(&fp, savename, FA_READ);
		if (res == FR_OK) {
			system_load_state(&fp);
		} else {
			ui_status_set("Could not load state %d", slot);
		}
	}

	f_close(&fp);
}

int ui_file_menu(const char* folder, char* filename) {
	DIR dp;
	FILINFO fno;
	FRESULT res;
	int file_count = 0;
	int cursor = 0;

	res = f_opendir(&dp, folder);
	if (res != FR_OK) {
		printf("couldn't open folder %s", folder);
		return 0;
	}

	for (;;) {
		res = f_readdir(&dp, &fno);           /* Read a directory item */
		if (fno.fname[0] == 0) break;          /* Error or end of dir */
		if (!(fno.fattrib & AM_DIR) && is_rom(fno.fname)) {
			file_count++;
		}
	}

	keyboard_flush();
	term_clear();

	char sbuf[128];
	sprintf(sbuf, "SMS Plus PicoCalc %s", GIT_DESC);
	center_print(1, sbuf);
	center_print(2, "Port by maple \"mavica\" syrup <maple@maple.pet>");
	center_print(3, "Original emulator by Charles MacDonald");
	int curoff = term_get_height() / 2;
	int page_size = term_get_height() - 8;
	
	while(true) {
		res = f_opendir(&dp, folder);
		
		int skip = cursor - curoff + 4;
		printf("\x1b[5;1H");
		for (int y = 4; y < 4 + page_size; y++) {
			if (y-curoff < -cursor) { printf("\x1b[K\n"); continue; };
			while (true) {
				res = f_readdir(&dp, &fno);           /* Read a directory item */
				if (!(fno.fattrib & AM_DIR) && is_rom(fno.fname)) {
					if (skip-- <= 0) break;
				};
				if (fno.fname[0] == 0) break;
			}
			if (fno.fname[0] == 0) { printf("\x1b[K\n"); continue; };          /* Error or end of dir */
			if (y == curoff) {
				printf("> ");
				sprintf(filename, "%s/%s", folder, fno.fname);
			}
			else printf("  ");
			printf("%s\x1b[K\n", fno.fname);
		}
		f_closedir(&dp);

		ui_status_print();
	
		input_event_t inkey = keyboard_wait_ex(true, true);

		if (inkey.code == KEY_UP) cursor = (cursor <= 0 ? file_count-1 : cursor - 1);
		else if (inkey.code == KEY_DOWN) cursor = (cursor >= file_count-1 ? 0 : cursor + 1);
		else if (inkey.code == KEY_LEFT) {
			cursor -= page_size  / 2;
			if (cursor < 0) cursor = 0;
		}
		else if (inkey.code == KEY_RIGHT) {
			cursor += page_size / 2;
			if (cursor >= file_count) cursor = file_count-1;
		}
		else if (inkey.code == KEY_ENTER) return 1;
	}
}

void ui_mainloop() {
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