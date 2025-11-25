#pragma once
#include "../pico_fatfs/fatfs/ff.h"

extern char* fs_error_strings[20];

void fs_init();
int fs_mount();
int fs_unmount();
int fs_exists(const char* path);
FRESULT fs_readline(FIL* fp, char** buffer, UINT* read);
FRESULT fs_writeline(FIL* fp, const char* line, UINT to_write, UINT* written);
void fs_check_hotplug();