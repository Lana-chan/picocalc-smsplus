/*
	state.c --
	Save state management.
*/

#include "shared.h"

#include "../picocalc_drivers/fs.h"

void system_save_state(void *fd)
{
	char *id = STATE_HEADER;
	uint16 version = STATE_VERSION;

	/* Write header */
	f_write(fd, id, sizeof(id), NULL);
	f_write(fd, &version, sizeof(version), NULL);

	/* Save VDP context */
	f_write(fd, &vdp, sizeof(vdp_t), NULL);

	/* Save SMS context */
	f_write(fd, &sms, sizeof(sms_t), NULL);

	f_write(fd, cart.fcr, 4, NULL);
#ifdef ENABLE_SRAM
	f_write(fd, cart.sram, 0x8000, NULL);
#else
	int zero = 0;
	for (int i = 0; i < 0x8000; i++) {
		f_write(fd, &zero, 1, NULL);
	}
#endif

	/* Save Z80 context */
	f_write(fd, Z80_Context, sizeof(Z80_Regs), NULL);
	f_write(fd, &after_EI, sizeof(int), NULL);

	/* Save YM2413 context */
	f_write(fd, FM_GetContextPtr(), FM_GetContextSize(), NULL);

	/* Save SN76489 context */
	f_write(fd, SN76489_GetContextPtr(0), SN76489_GetContextSize(), NULL);
}


void system_load_state(void *fd)
{
	int i;
	uint8 *buf;
	char id[4];
	uint16 version;

	/* Initialize everything */
	z80_reset(0);
	z80_set_irq_callback(sms_irq_callback);
	system_reset();
	if(snd.enabled)
		sound_reset();

	/* Read header */
	f_read(fd, id, sizeof(id), NULL);
	f_read(fd, &version, sizeof(version), NULL);

	/* Load VDP context */
	f_read(fd, &vdp, sizeof(vdp_t), NULL);

	/* Load SMS context */
	f_read(fd, &sms, sizeof(sms_t), NULL);

	f_read(fd, cart.fcr, 4, NULL);
#ifdef ENABLE_SRAM
	fread(cart.sram, 0x8000, 1, fd);
#else
	f_lseek(fd, ((FIL*)fd)->fptr + 0x8000);
#endif

	/* Load Z80 context */
	f_read(fd, Z80_Context, sizeof(Z80_Regs), NULL);
	f_read(fd, &after_EI, sizeof(int), NULL);

	/* Load YM2413 context */
	buf = malloc(FM_GetContextSize());
	f_read(fd, buf, FM_GetContextSize(), NULL);
	FM_SetContext(buf);
	free(buf);

	/* Load SN76489 context */
	buf = malloc(SN76489_GetContextSize());
	f_read(fd, buf, SN76489_GetContextSize(), NULL);
	SN76489_SetContext(0, buf);
	free(buf);

	/* Restore callbacks */
	z80_set_irq_callback(sms_irq_callback);

	// don't we need to account for banking?
	for(i = 0x00; i <= 0x2F; i++)
	{
		cpu_readmap[i]  = &cart.rom[(i & 0x1F) << 10];
		cpu_writemap[i] = dummy_write;
	}

	for(i = 0x30; i <= 0x3F; i++)
	{
		cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
		cpu_writemap[i] = &sms.wram[(i & 0x07) << 10];
	}

	sms_mapper_w(3, cart.fcr[3]);
	sms_mapper_w(2, cart.fcr[2]);
	sms_mapper_w(1, cart.fcr[1]);
	sms_mapper_w(0, cart.fcr[0]);

	/* Force full pattern cache update */
	bg_list_index = 0x200;
	for(i = 0; i < 0x200; i++)
	{
		bg_name_list[i] = i;
		bg_name_dirty[i] = -1;
	}

	/* Restore palette */
	for(i = 0; i < PALETTE_SIZE; i++)
		palette_sync(i, 1);
}

