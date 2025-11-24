/*
	fmintf.c --
	Interface to EMU2413 and YM2413 emulators.
*/
#include "shared.h"

FM_Context fm_context;

void FM_Init(void)
{
#ifdef ENABLE_FM
	switch(snd.fm_which)
	{
		case SND_YM2413:
			YM2413Init(1, snd.fm_clock, snd.sample_rate);
			YM2413ResetChip(0);
			break;
	}
#endif
}


void FM_Shutdown(void)
{
#ifdef ENABLE_FM
	switch(snd.fm_which)
	{
		case SND_YM2413:
			YM2413Shutdown();
			break;
	}
#endif
}


void FM_Reset(void)
{
#ifdef ENABLE_FM
	switch(snd.fm_which)
	{
		case SND_YM2413:
			YM2413ResetChip(0);
			break;
		}
#endif
}


void FM_Update(int16 **buffer, int length)
{
#ifdef ENABLE_FM
	switch(snd.fm_which)
	{
		case SND_YM2413:
			YM2413UpdateOne(0, buffer, length);
			break;
	}
#endif
}

void FM_WriteReg(int reg, int data)
{
#ifdef ENABLE_FM
	FM_Write(0, reg);
	FM_Write(1, data);
#endif
}

void FM_Write(int offset, int data)
{
#ifdef ENABLE_FM
	if(offset & 1)
		fm_context.reg[ fm_context.latch ] = data;
	else
		fm_context.latch = data;

	switch(snd.fm_which)
	{
		case SND_YM2413:
			YM2413Write(0, offset & 1, data);
			break;
	}
#endif
}


void FM_GetContext(uint8 *data)
{
#ifdef ENABLE_FM
	memcpy(data, &fm_context, sizeof(FM_Context));
#endif
}

void FM_SetContext(uint8 *data)
{
#ifdef ENABLE_FM
	int i;
	uint8 *reg = fm_context.reg;

	memcpy(&fm_context, data, sizeof(FM_Context));

	/* If we are loading a save state, we want to update the YM2413 context
	   but not actually write to the current YM2413 emulator. */
	if(!snd.enabled || !sms.use_fm)
		return;

	FM_Write(0, 0x0E);
	FM_Write(1, reg[0x0E]);

	for(i = 0x00; i <= 0x07; i++)
	{
		FM_Write(0, i);
		FM_Write(1, reg[i]);
	}

	for(i = 0x10; i <= 0x18; i++)
	{
		FM_Write(0, i);
		FM_Write(1, reg[i]);
	}

	for(i = 0x20; i <= 0x28; i++)
	{
		FM_Write(0, i);
		FM_Write(1, reg[i]);
	}

	for(i = 0x30; i <= 0x38; i++)
	{
		FM_Write(0, i);
		FM_Write(1, reg[i]);
	}

	FM_Write(0, fm_context.latch);
#endif
}

int FM_GetContextSize(void)
{
#ifdef ENABLE_FM
	return sizeof(FM_Context);
#endif
}

uint8 *FM_GetContextPtr(void)
{
#ifdef ENABLE_FM
	return (uint8 *)&fm_context;
#endif
}
