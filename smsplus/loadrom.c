/*
	loadrom.c --
	File loading and management.
*/

#include "shared.h"

#include "../picocalc_drivers/keyboard.h"
#include "../picocalc_drivers/flash.h"
#include "../picocalc_drivers/multicore.h"

#include "hardware/flash.h"
#include "hardware/sync.h"

#define FLASH_CART_SIZE (512 * 1024)

#define RoundUpK4(a)     (((a) + (4096 - 1)) & (~(4096 - 1))) // round up to the nearest page size

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

void __not_in_flash_func(NullCore)() {
	flash_safe_execute_core_init(); // allow flash operations to lock out this core
	while (true) tight_loop_contents();
}

int load_rom(char *filename)
{
	int i;
	int size;

	FIL fd;
	FRESULT res;
	int fd_skip;

	res = f_open(&fd, filename, FA_READ);
	if(res != FR_OK) return 0;

	/* Get size */
	size = f_size(&fd);
	
	/* Don't load games smaller than 16K */
	if(size < PAGE_SIZE) return 0;
	
	/* Don't load games larger than 512K */
	if(size > 512 * 1024) return 0;
	
	/* Take care of image header, if present */
	fd_skip = 0;
	if((size / 512) & 1)
	{
		size -= 512;
		fd_skip = 512;
	}
	
	cart.pages = (size / PAGE_SIZE);
	//cart.crc = crc32(0L, cart.rom, size);

	res = f_lseek(&fd, fd_skip);

	printf("Loading...\n");

	uintptr_t flash_cart_addr = RoundUpK4(bl_proginfo_flash_size() - FLASH_CART_SIZE - 4095);

	printf("%d\n", flash_cart_addr);

	multicore_reset_core1();
	multicore_launch_core1(NullCore);
	busy_wait_ms(100);

	flash_erase(flash_cart_addr, FLASH_CART_SIZE);
	busy_wait_ms(100);

	uint8 buf[FLASH_PAGE_SIZE];
	for (int i = 0; i < size; i += FLASH_PAGE_SIZE) {
		res = f_read(&fd, buf, FLASH_PAGE_SIZE, NULL);
		flash_program(flash_cart_addr + i, buf, FLASH_PAGE_SIZE);
		busy_wait_ms(100);
	}

	res = f_close(&fd);

	cart.rom = (uint8*)(XIP_BASE + flash_cart_addr);

	printf("%d\n%d\n", XIP_BASE, cart.rom);

	keyboard_wait_ex(true, true);

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
}

