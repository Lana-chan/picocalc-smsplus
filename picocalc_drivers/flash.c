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

// thread to do nothing but allow rom_flash_op to work
void __not_in_flash_func(NullCore)()
{
	flash_safe_execute_core_init(); // allow flash operations to lock out this core

	while (true)
	{
		tight_loop_contents();
	}
}

void __not_in_flash_func(ResetCore)()
{
	flash_safe_execute_core_deinit();

	while (true)
	{
		tight_loop_contents();
	}
}

int flash_erase(uint32_t address, uint32_t size_bytes)
{
	// Round up size_bytes or rom_flash_op will throw an alignment error
	uint32_t size_aligned = (size_bytes + 0x1FFF) & -FLASH_SECTOR_SIZE;

#if PICO_RP2040
	flash_range_erase(address, size_aligned);
	
#elif PICO_RP2350
	uint32_t addr = rom_flash_runtime_to_storage_addr(XIP_BASE + address);
	rom_connect_internal_flash();
	rom_flash_range_erase(addr, size_aligned, 0, 0);

	// no matter what, rom_flash_op does not allow core1 to ever be used again. i cannot find out why. i give up
/*	cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB) |
								   (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
								   (CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB)};

	int ret = rom_flash_op(cflash_flags, address + XIP_BASE, size_aligned, NULL);

	if (ret != PICO_OK)
	{
		// need to debug all of these
		while(1);
	}

	return ret;*/
#endif
	rom_flash_flush_cache();

	busy_wait_ms(300);
	return 0;
}

int flash_program(uint32_t address, const void* buf, uint32_t size_bytes)
{
#if PICO_RP2040
	flash_range_program(address, buf, size_bytes);

#elif PICO_RP2350
	uint32_t addr = rom_flash_runtime_to_storage_addr(XIP_BASE + address);
	rom_connect_internal_flash();
	rom_flash_range_program(addr, buf, size_bytes);

/*	cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB) |
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

	return ret;*/
#endif
	rom_flash_flush_cache();

	busy_wait_ms(2);
	return 0;
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