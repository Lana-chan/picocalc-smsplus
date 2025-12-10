#pragma once

#include <stdbool.h>

extern bool ui_status_updated;

void ui_status_set(const char* fmt, ...);
void ui_status_print();
void ui_handle_savestate(int slot, int save);
void ui_mainloop();