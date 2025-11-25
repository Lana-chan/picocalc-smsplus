/*
	loadrom.c --
	File loading and management.
*/

#include "shared.h"

#include "../picocalc_drivers/keyboard.h"

#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#define FLASH_CART_SIZE (512 * 1024)
#define FLASH_CART_ADDR (PICO_FLASH_SIZE_BYTES - FLASH_CART_SIZE - FLASH_PAGE_SIZE) // write cartridge 512k away from the end of flash, plus page

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

static void call_flash_cart_erase(void *param) {
	flash_range_erase(FLASH_CART_ADDR, FLASH_CART_SIZE);
}

static void call_flash_cart_program(void *param) {
	uint32_t offset = ((uintptr_t*)param)[0];
	const uint8_t *data = (const uint8_t *)((uintptr_t*)param)[1];
	flash_range_program(FLASH_CART_ADDR + offset, data, FLASH_PAGE_SIZE);
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

	printf("Loading...");

	keyboard_enable_timer(false);
	uint32_t ints = save_and_disable_interrupts();

	//int rc = flash_safe_execute(call_flash_cart_erase, NULL, UINT32_MAX);
	//printf("\n%d\n", rc);
	//hard_assert(rc == PICO_OK);
	flash_range_erase(FLASH_CART_ADDR, FLASH_CART_SIZE);

	uint8 buf[FLASH_PAGE_SIZE];
	for (int i = 0; i < size; i += FLASH_PAGE_SIZE) {
		res = f_read(&fd, buf, FLASH_PAGE_SIZE, NULL);
		//uintptr_t params[] = { i * FLASH_PAGE_SIZE, (uintptr_t)buf};
		//rc = flash_safe_execute(call_flash_cart_program, params, UINT32_MAX);
		//hard_assert(rc == PICO_OK);
		flash_range_program(FLASH_CART_ADDR + i, buf, FLASH_PAGE_SIZE);
	}

	restore_interrupts(ints);
	keyboard_enable_timer(true);

	res = f_close(&fd);

	cart.rom = (uint8*)(XIP_BASE + FLASH_CART_ADDR);

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

