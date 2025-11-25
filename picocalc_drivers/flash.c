// Abstract flash operations on RP2040/RP2350
// James Churchill 2025

#include "hardware/flash.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "flash.h"

#if PICO_RP2040

#define VECTOR_HOLE_OFFSET 0x110
#define VECTOR_HOLE_SIZE 0x1C

#elif PICO_RP2350

#include "hardware/structs/qmi.h"
#define VECTOR_HOLE_OFFSET 0x20
#define VECTOR_HOLE_SIZE 0x0C

#endif

#define PICOCALC_PROGINFO_ADDR (XIP_BASE + VECTOR_HOLE_OFFSET)
#define PICOCALC_BLINFO_ADDR (SRAM_BASE + VECTOR_HOLE_OFFSET)

// this info is stashed in the vector table in a gap that is not used by the Cortex-M0+.
// Apps *shouldn't* be putting data in there but you never know...

static_assert(sizeof(struct bl_info_t) <= VECTOR_HOLE_SIZE,
			  "Proginfo struct too large for vector table hole\n");

static struct bl_info_t *proginfo = (void *)PICOCALC_PROGINFO_ADDR;

bool bl_proginfo_valid(void) { return (proginfo->magic == PICOCALC_BL_MAGIC); }

int flash_erase(uint32_t address, uint32_t size_bytes)
{
#if PICO_RP2040
	flash_range_erase(address, size_bytes);
	return 0;

#elif PICO_RP2350
	cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB) |
								   (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
								   (CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB)};

	// Round up size_bytes or rom_flash_op will throw an alignment error
	uint32_t size_aligned = (size_bytes + 0x1FFF) & -FLASH_SECTOR_SIZE;

	int ret = rom_flash_op(cflash_flags, address + XIP_BASE, size_aligned, NULL);

	if (ret != PICO_OK)
	{
		// need to debug all of these
		while(1);
	}

	rom_flash_flush_cache();

	return ret;
#endif
}

int flash_program(uint32_t address, const void* buf, uint32_t size_bytes)
{
#if PICO_RP2040
	flash_range_program(address, buf, size_bytes);
	return 0;

#elif PICO_RP2350
	cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB) |
									(CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
									(CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB)};

	// Round up size_bytes or rom_flash_op will throw an alignment error
	uint32_t size_aligned = (size_bytes + 255) & -FLASH_PAGE_SIZE;

	int ret = rom_flash_op(cflash_flags, address + XIP_BASE, size_aligned, (void*)buf);

	if (ret != PICO_OK)
	{
		// need to debug all of these
		while(1);
	}

	rom_flash_flush_cache();

	return ret;
#endif
}

size_t bl_proginfo_flash_size(void)
{
	if (bl_proginfo_valid())
	{
		return proginfo->flash_end;
	}

	#if PICO_RP2040
	return PICO_FLASH_SIZE_BYTES - 4 * FLASH_SECTOR_SIZE;
	#else
	uint32_t offset = ((qmi_hw->atrans[0] & QMI_ATRANS0_BASE_BITS) >> QMI_ATRANS0_BASE_LSB) * FLASH_SECTOR_SIZE;
	return PICO_FLASH_SIZE_BYTES - offset;
	#endif
}