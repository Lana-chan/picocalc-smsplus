#pragma once

#include <stdbool.h>

typedef struct {
	bool enable_fm;
	bool enable_double;
	bool disable_flicker;
	int frameskip;
} options_t;

extern options_t options;