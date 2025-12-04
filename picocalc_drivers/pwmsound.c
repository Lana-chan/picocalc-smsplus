#include "pwmsound.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static uint sound_slice;
static uint32_t sound_wrap_value;

static int sound_dma_chan;
static int sound_dma_timer;
static dma_channel_config sound_dma_config;

static int16_t* sound_buffer_source;
static uint16_t* sound_buffer;
static int buffer_to_write;
static int buffer_to_read;
static int sound_buffer_size;

#define SGN(x) (x > 0 ? 1 : (x < 0 ? -1 : 0))
#define ABS(x) (x > 0 ? x : -x)
#define CLAMP(x, a, b) (x < a ? a : (x > b ? b : x))

void sound_dma_handler(void);

void pwmsound_fillbuffer_local() {
	uint16_t* buffer = &sound_buffer[buffer_to_write * sound_buffer_size];
	for (uint16_t i = 0; i < sound_buffer_size; i++) {
		int32_t out = BITDEPTH / 2 + 1;
		
		out += sound_buffer_source[i] * 4;

		out = CLAMP(out, 0, BITDEPTH);
		buffer[i] = (sound_wrap_value-1) * out / (BITDEPTH);
	}
	buffer_to_write = (buffer_to_write + 1) % BUFFER_COUNT;
}

void pwmsound_clearbuffer_local() {
	for (int n = 0; n < BUFFER_COUNT; n++) {
		uint16_t* buffer = &sound_buffer[n * sound_buffer_size];
		for (uint16_t i = 0; i < sound_buffer_size; i++) {
			int32_t out = BITDEPTH / 2 + 1;
			out = CLAMP(out, 0, BITDEPTH);
			buffer[i] = (sound_wrap_value-1) * out / (BITDEPTH);
		}
	}
}

void pwmsound_enabledma_local(bool enable) {
	irq_set_enabled(DMA_IRQ_1, enable);
	dma_channel_set_irq1_enabled(sound_dma_chan, enable);
	if (enable) sound_dma_handler();
}

static void sound_initialbuffer() {
	// ramp from 0 to dc middle to diminish pop when booting
	uint16_t* buffer = &sound_buffer[buffer_to_write * sound_buffer_size];
	for (uint16_t i = 0; i < sound_buffer_size; i++) {
		sound_buffer[i] = (sound_wrap_value-1)/2 * i / sound_buffer_size;
	}
	buffer_to_write = (buffer_to_write + 1) % BUFFER_COUNT;
}

void sound_dma_handler(void) {
	dma_hw->ints0 = 1u << sound_dma_chan;
	dma_channel_set_read_addr(sound_dma_chan, sound_buffer + buffer_to_read * sound_buffer_size, true);
	if (buffer_to_write != (buffer_to_read + 1) % BUFFER_COUNT) buffer_to_read = (buffer_to_read + 1) % BUFFER_COUNT;
	//sound_fillbuffer();
}

void pwmsound_init() {
	gpio_set_function(AUDIO_PIN_L, GPIO_FUNC_PWM);
	gpio_set_function(AUDIO_PIN_R, GPIO_FUNC_PWM);
	sound_slice = pwm_gpio_to_slice_num(AUDIO_PIN_L); // 26 and 27 are in same slice, different channels
	sound_wrap_value = 1024;
	pwm_set_clkdiv_int_frac(sound_slice, 1, 0);
	pwm_set_wrap(sound_slice, sound_wrap_value-1);

	sound_dma_chan = dma_claim_unused_channel(true);
	sound_dma_timer = dma_claim_unused_timer(true);
	sound_dma_config = dma_channel_get_default_config(sound_dma_chan);
	channel_config_set_transfer_data_size(&sound_dma_config, DMA_SIZE_16); // PWM HW register is 32bit, sending 16bit relpicates same sound on both channels
	channel_config_set_read_increment(&sound_dma_config, true);
	channel_config_set_write_increment(&sound_dma_config, false);
	channel_config_set_dreq(&sound_dma_config, dma_get_timer_dreq(sound_dma_timer));
	pwmsound_setclk();

	pwm_set_enabled(sound_slice, true);

	irq_set_exclusive_handler(DMA_IRQ_1, sound_dma_handler);
	irq_set_priority(DMA_IRQ_1, PICO_DEFAULT_IRQ_PRIORITY - 10);
}

void pwmsound_setclk() {
	// https://github.com/tannewt/circuitpython/blob/191b143e7ba3b9f5786996cbffb42e2460f1c14d/ports/raspberrypi/common-hal/audiopwmio/PWMAudioOut.c#L178
  uint32_t sample_rate = BITRATE;
	uint32_t system_clock = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS)*1000;
	uint32_t best_numerator = 0;
	uint32_t best_denominator = 0;
	uint32_t best_error = system_clock;
	for (uint32_t denominator = 0xffff; denominator > 0; denominator--) {
		uint32_t numerator = (denominator * sample_rate) / system_clock;
		uint32_t remainder = (denominator * sample_rate) % system_clock;
		if (remainder > (system_clock / 2)) {
			numerator += 1;
			remainder = system_clock - remainder;
		}
		if (remainder < best_error) {
			best_denominator = denominator;
			best_numerator = numerator;
			best_error = remainder;
			// Stop early if we can't do better.
			if (remainder == 0) {
				break;
			}
		}
	}
	dma_timer_set_fraction(sound_dma_timer, best_numerator, best_denominator);
}

void pwmsound_register_buffer(int16_t* buffer, int length) {
	if (sound_buffer) {
		free(sound_buffer);
		sound_buffer = NULL;
	}

	sound_buffer_source = buffer;
	volatile int total_buffer = length * BUFFER_COUNT * sizeof(uint16_t);
	sound_buffer = malloc(total_buffer);
	memset(sound_buffer, 0, total_buffer);
	sound_buffer_size = length;
	buffer_to_read = 0;
	buffer_to_write = 0;

	dma_channel_configure(
		sound_dma_chan,
		&sound_dma_config,
		&pwm_hw->slice[sound_slice].cc,
		0,
		sound_buffer_size,
		false
	);

	sound_initialbuffer();
	pwmsound_enabledma(true);
}

int pwmsound_fifo_receiver(uint32_t message) {
	switch (message) {
		case FIFO_PWM_FILLBUF:
			pwmsound_fillbuffer_local();
			return 1;

		case FIFO_PWM_CLEARBUF:
			pwmsound_clearbuffer_local();
			return 1;

		case FIFO_PWM_ENABLEDMA:
			bool enable = (bool)multicore_queue_pop();
			pwmsound_enabledma_local(enable);
			return 1;

		default:
			return 0;
	}
}