#include <stdio.h>
#include <stdbool.h>
#include "types.h"
#include "sms.h"
#include "shared.h"
#include "picocalc_drivers/keyboard.h"
#include "picocalc_drivers/pwmsound.h"
#include "picocalc_drivers/term.h"
#include "pico/time.h"

#ifdef BAKED_ROM
#include "rom.h"
#endif

#define _60HZ_US 16667 // 16.667ms
#define _SKIP_THRESH_US 100
#define _1SEC_US 1000000

bool shutdown = false;

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
}

static bool sms_frame(repeating_timer_t *rt) {
	last = get_absolute_time();
	sms_input();
	system_frame(skip);
	pwmsound_fillbuffer();
	//skip = (absolute_time_diff_us(last, get_absolute_time()) > _60HZ_US + _SKIP_THRESH_US);
	skip = fps_count % 4;
	fps_count++;
	if (absolute_time_diff_us(fps_time, get_absolute_time()) >= _1SEC_US) {
		term_set_pos(0,32);
		printf("fps: %d   ", fps_count);
		fps_count = 0;
		fps_time = get_absolute_time();
	}
	return true;
}

void sms_main() {
#ifdef BAKED_ROM
	size_t size = sizeof(sms_rom);
	cart.rom = (uint8*)sms_rom;
	if((size / 512) & 1)
	{
		size -= 512;
		cart.rom = (uint8*)(sms_rom + 512);
	}

	cart.mapper     = MAPPER_SEGA;
	sms.display     = DISPLAY_NTSC;
	sms.territory   = TERRITORY_EXPORT;
	sms.console = CONSOLE_SMS;
	system_assign_device(PORT_A, DEVICE_PAD2B);
	system_assign_device(PORT_B, DEVICE_PAD2B);
#else
	if (!load_rom("/sms/Hang-On II.sg")) {
		printf("couldn't load\n");
		system_poweroff();
		return;
	}
#endif

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

	while (1) {
		if (shutdown) break;
	}

	system_poweroff();
}