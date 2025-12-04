#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "multicore.h"

#define AUDIO_PIN_L 26
#define AUDIO_PIN_R 27

#define BITRATE  16000 // in hz
#define BUFFER_COUNT 4
#define BITDEPTH 65535 // 16bit samples

int pwmsound_fifo_receiver(uint32_t message);

void pwmsound_init();
void pwmsound_setclk();
void pwmsound_fillbuffer_local();
void pwmsound_clearbuffer_local();
void pwmsound_enabledma_local(bool enable);
void pwmsound_register_buffer(int16_t* buffer, int length);

static inline void pwmsound_fillbuffer() {
	if (get_core_num() == 0) pwmsound_fillbuffer_local();
	else {
		mutlicore_queue_push(FIFO_PWM_FILLBUF);
	}
}

static inline void pwmsound_clearbuffer() {
	if (get_core_num() == 0) pwmsound_clearbuffer_local();
	else {
		mutlicore_queue_push(FIFO_PWM_CLEARBUF);
	}
}

static inline void pwmsound_enabledma(bool enable) {
	if (get_core_num() == 0) pwmsound_enabledma_local(enable);
	else {
		mutlicore_queue_push(FIFO_PWM_ENABLEDMA);
		mutlicore_queue_push(enable);
	}
}

