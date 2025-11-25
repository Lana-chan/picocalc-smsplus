#ifndef _LOADROM_H_
#define _LOADROM_H_

/* Function prototypes */
void load_psram_init();
void load_rom_bank_page(int bank, int page);
int load_rom(char *filename);
//unsigned char *loadzip(char *archive, char *filename, int *filesize);

#endif /* _LOADROM_H_ */

