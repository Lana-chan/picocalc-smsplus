// Abstract flash operations on RP2040/RP2350
// James Churchill 2025

#define PICOCALC_BL_MAGIC 0xe98cc638

struct bl_info_t
{
	uint32_t magic;
	uint32_t flash_end;
#if PICO_RP2040
	char filename[20];  // RP2040 only!
#endif
};

int flash_erase(uint32_t address, uint32_t size_bytes);
int flash_program(uint32_t address, const void* buf, uint32_t size_bytes);
size_t bl_proginfo_flash_size(void);
