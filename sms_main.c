#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "sms.h"
#include "sms_ui.h"
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
volatile bool emu_running = false;

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
	memset(&input, 0, sizeof(input));
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
	if (fps_count % 60 == 0) ui_status_print();
	return !shutdown;
}

void sms_play_rom() {
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

	cancel_repeating_timer(&sms_timer);
	pwmsound_clearbuffer();
	pwmsound_enabledma(false);
	
	system_poweroff();
	system_shutdown();
	keyboard_enable_queue(true);

	emu_running = false;

	return;
}

void sms_main() {
	emu_running = true;

	multicore_launch_core1(sms_play_rom);

	while (emu_running) tight_loop_contents();

	multicore_reset_core1();
}