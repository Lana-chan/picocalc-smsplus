#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "sms.h"
#include "shared.h"
#include "picocalc_drivers/keyboard.h"
#include "picocalc_drivers/pwmsound.h"
#include "picocalc_drivers/term.h"
#include "pico/time.h"

#define _60HZ_US 16667 // 16.667ms
#define _SKIP_THRESH_US 100
#define _1SEC_US 1000000

#define MAX_FRAMESKIP 6

volatile bool shutdown = false;

// this will get better in the future i promise
#ifdef PICO_RP2040
int frameskip = 4;
#endif
#ifdef PICO_RP2350
int frameskip = 2;
#endif

int skip;
absolute_time_t last;
absolute_time_t fps_time;
int fps_count = 0;
static repeating_timer_t sms_timer;

void sms_input() {
	input.pad[0] = 0;
	input.system = 0;
	if (keyboard_states[KEY_UP]) input.pad[0] |= INPUT_UP;
	if (keyboard_states[KEY_DOWN]) input.pad[0] |= INPUT_DOWN;
	if (keyboard_states[KEY_LEFT]) input.pad[0] |= INPUT_LEFT;
	if (keyboard_states[KEY_RIGHT]) input.pad[0] |= INPUT_RIGHT;
	if (keyboard_states['[']) input.pad[0] |= INPUT_BUTTON1;
	if (keyboard_states[']']) input.pad[0] |= INPUT_BUTTON2;
	if (keyboard_states[KEY_ENTER]) input.system |= INPUT_START;
	if (keyboard_states[KEY_F1]) input.system |= INPUT_PAUSE;
	if (keyboard_states[KEY_F2]) input.system |= INPUT_RESET;
	if (keyboard_states[KEY_ESC]) shutdown = true;
}

static bool sms_frame(repeating_timer_t *rt) {
	last = get_absolute_time();
	sms_input();
	system_frame(skip);
	pwmsound_fillbuffer();
	//skip = (absolute_time_diff_us(last, get_absolute_time()) > _60HZ_US + _SKIP_THRESH_US);
	skip = fps_count % frameskip;
	fps_count++;
	/*term_set_pos(0,32);
	if (absolute_time_diff_us(fps_time, get_absolute_time()) >= _1SEC_US) {
		printf("fps: %d   ", fps_count);
		fps_count = 0;
		fps_time = get_absolute_time();
	}*/
	return !shutdown;
}

bool is_rom(const char* filename) {
	if (strcasecmp(strrchr(filename, '.'), ".sms") == 0) return true;
	if (strcasecmp(strrchr(filename, '.'), ".gg") == 0) return true;
	if (strcasecmp(strrchr(filename, '.'), ".sg") == 0) return true;
	return false;
}

int sms_file_menu(const char* folder, char* filename) {
	DIR dp;
	FILINFO fno;
	FRESULT res;
	int file_count = 0;
	int cursor = 0;

	res = f_opendir(&dp, folder);
	if (res != FR_OK) {
		printf("couldn't open folder %s", folder);
		return -1;
	}

	for (;;) {
		res = f_readdir(&dp, &fno);           /* Read a directory item */
		if (fno.fname[0] == 0) break;          /* Error or end of dir */
		if (!(fno.fattrib & AM_DIR) && is_rom(fno.fname)) {
			file_count++;
		}
	}

	while(true) {
		term_clear();
		res = f_opendir(&dp, folder);
		int i = 0;

		printf(" <- frameskip: %d ->  \n\n", frameskip);
	
		for (;;) {
			res = f_readdir(&dp, &fno);           /* Read a directory item */
			if (fno.fname[0] == 0 || i >= 30) break;          /* Error or end of dir */
			if (!(fno.fattrib & AM_DIR) && is_rom(fno.fname)) {
				i++;
				if (i-1 < cursor) continue;
				if (i-1 == cursor) {
					printf("> ");
					sprintf(filename, "%s/%s", folder, fno.fname);
				}
				else printf("  ");
				printf("%s\x1b[K\n", fno.fname);
			}
		}
	
		f_closedir(&dp);
	
		input_event_t inkey = keyboard_wait_ex(true, true);

		if (inkey.code == KEY_UP) cursor = (cursor <= 0 ? file_count-1 : cursor - 1);
		if (inkey.code == KEY_DOWN) cursor = (cursor >= file_count-1 ? 0 : cursor + 1);
		if (inkey.code == KEY_LEFT) frameskip = (frameskip <= 0 ? MAX_FRAMESKIP-1 : frameskip - 1);
		if (inkey.code == KEY_RIGHT) frameskip = (frameskip >= MAX_FRAMESKIP-1 ? 0 : frameskip + 1);
		if (inkey.code == KEY_ENTER) return 0;
	}
}

void sms_play_rom(char* filename) {
	term_clear();

	if (!load_rom(filename)) {
		printf("couldn't load\n");
		return;
	}
	
	term_clear();

	shutdown = false;
	snd.sample_rate = BITRATE;
	snd.fps = FPS_NTSC;
	snd.fm_which = SND_YM2413;
	snd.psg_clock = CLOCK_NTSC;
	snd.fm_clock = CLOCK_NTSC;

	system_init();
	if (snd.enabled) pwmsound_register_buffer(snd.output[0], snd.sample_count);
	system_poweron();

	last = nil_time;
	fps_time = nil_time;
	add_repeating_timer_us(-_60HZ_US, sms_frame, NULL, &sms_timer);

	keyboard_enable_queue(false);

	while (!shutdown) tight_loop_contents();
	pwmsound_clearbuffer();
	pwmsound_enabledma(false);

	cancel_repeating_timer(&sms_timer);
	
	system_poweroff();
	system_shutdown();
	keyboard_enable_queue(true);
}

void sms_main() {
	flash_safe_execute_core_init();
	char filename[256];
	int status;
	while (1) {
		status = sms_file_menu("/sms", filename);
		if (status) break;
		sms_play_rom(filename);
	}
}