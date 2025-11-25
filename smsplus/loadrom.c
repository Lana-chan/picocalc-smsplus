/*
	loadrom.c --
	File loading and management.
*/

#include "shared.h"
#include "../rp2040-psram/psram_spi.h"

#define PSRAM_WRITE_BYTES 64
#define PSRAM_READ_BYTES 128

psram_spi_inst_t psram_spi;
psram_spi_inst_t* async_spi_inst;

typedef struct {
	uint32 crc;
	int mapper;
	int display;
	int territory;
	char *name;
} rominfo_t;

rominfo_t game_list[] = {
	{0x29822980, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Cosmic Spacehead"},
	{0xB9664AE1, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Fantastic Dizzy"},
	{0xA577CE46, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Micro Machines"},
	{0x8813514B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Excellent Dizzy (Proto)"},
	{0xAA140C9C, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Excellent Dizzy (Proto - GG)"}, 
	{-1        , -1           , -1         , -1              , NULL},
};

void load_psram_init() {
	psram_spi = psram_spi_init_clkdiv(pio0, -1, 1.4, true); // fastest clkdiv safe for RP2040 250MHZ
}

void load_rom_bank_page(int bank, int page)
{
#ifndef BAKED_ROM
	//FRESULT res;
	page %= cart.pages;
	//printf("\x1b[1;1Hbank load %d, %d\x1b[K", bank, page);
	/*res = f_lseek(&cart.fd, cart.fd_skip + page * PAGE_SIZE);
	res = f_read(&cart.fd, cart.banks[bank], PAGE_SIZE, NULL);*/
	for (int i = 0; i < PAGE_SIZE; i += PSRAM_READ_BYTES) {
		psram_read(&psram_spi, page * PAGE_SIZE + i, &cart.banks[bank][i], PSRAM_READ_BYTES);
	}
#endif
}

int load_rom(char *filename)
{
#ifndef BAKED_ROM
	int i;
	int size;

	FRESULT res;

	res = f_open(&cart.fd, filename, FA_READ);
	if(res != FR_OK) return 0;

	/* Get size */
	size = f_size(&cart.fd);
	
	/* Don't load games smaller than 16K */
	if(size < PAGE_SIZE) return 0;
	
	/* Take care of image header, if present */
	cart.fd_skip = 0;
	if((size / 512) & 1)
	{
		size -= 512;
		cart.fd_skip = 512;
	}
	
	cart.pages = (size / PAGE_SIZE);
	//cart.crc = crc32(0L, cart.rom, size);

	res = f_lseek(&cart.fd, cart.fd_skip);
	res = f_read(&cart.fd, cart.static_bank, sizeof(cart.static_bank), NULL);

	res = f_lseek(&cart.fd, cart.fd_skip);
	uint8 buf[PSRAM_WRITE_BYTES];
	for (int i = 0; i < size; i += PSRAM_WRITE_BYTES) {
		res = f_read(&cart.fd, buf, PSRAM_WRITE_BYTES, NULL);
		psram_write(&psram_spi, i, buf, PSRAM_WRITE_BYTES);
	}

	res = f_close(&cart.fd);

	load_rom_bank_page(0, 0);
	load_rom_bank_page(1, 1);
	load_rom_bank_page(2, 0); // initial behaviour is to mirror first 32k

	/* Assign default settings (US NTSC machine) */
	cart.mapper     = MAPPER_SEGA;
	sms.display     = DISPLAY_NTSC;
	sms.territory   = TERRITORY_EXPORT;

	/* Look up mapper in game list */
	for(i = 0; game_list[i].name != NULL; i++)
	{
		if(cart.crc == game_list[i].crc)
		{
			cart.mapper     = game_list[i].mapper;
			sms.display     = game_list[i].display;
			sms.territory   = game_list[i].territory;
		}
	}

	/* Figure out game image type */
	if(strcasecmp(strrchr(filename, '.'), ".gg") == 0)
		sms.console = CONSOLE_GG;
	else
		sms.console = CONSOLE_SMS;

	system_assign_device(PORT_A, DEVICE_PAD2B);
	system_assign_device(PORT_B, DEVICE_PAD2B);

	return 1;
#endif
}

