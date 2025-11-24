#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define LSB_FIRST 1

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

uint8 lut[0x10000];     /* Pixel look-up table */
uint32 bp_lut[0x10000]; /* Bitplane to packed pixel LUT */

uint8 tms_lookup[16][256][2];   /* Expand BD, PG data into 8-bit pixels (G1,G2) */
uint8 mc_lookup[16][256][8];    /* Expand BD, PG data into 8-bit pixels (MC) */
uint8 txt_lookup[256][2];       /* Expand BD, PG data into 8-bit pixels (TX) */
uint8 bp_expand[256][8];        /* Expand PG data into 8-bit pixels */
uint8 tms_obj_lut[16*256];      /* Look up priority between SG and display pixels */

void make_render_luts() {
	int i, j;
	int bx, sx, b, s, bp, bf, sf, c;

	/* Generate 64k of data for the look up table */
	for(bx = 0; bx < 0x100; bx++)
	{
		for(sx = 0; sx < 0x100; sx++)
		{
			/* Background pixel */
			b  = (bx & 0x0F);

			/* Background priority */
			bp = (bx & 0x20) ? 1 : 0;

			/* Full background pixel + priority + sprite marker */
			bf = (bx & 0x7F);

			/* Sprite pixel */
			s  = (sx & 0x0F);

			/* Full sprite pixel, w/ palette and marker bits added */
			sf = (sx & 0x0F) | 0x10 | 0x40;

			/* Overwriting a sprite pixel ? */
			if(bx & 0x40)
			{
				/* Return the input */
				c = bf;
			}
			else
			{
					/* Work out priority and transparency for both pixels */
				if(bp)
				{
					/* Underlying pixel is high priority */
					if(b)
					{
						c = bf | 0x40;
					}
					else
					{
							
						if(s)
						{
							c = sf;
						}
						else
						{
							c = bf;
						}
					}
				}
				else
				{
					/* Underlying pixel is low priority */
					if(s)
					{
						c = sf;
					}
					else
					{
						c = bf;
					}
				}
			}

			/* Store result */
			lut[(bx << 8) | (sx)] = c;
		}
	}

	/* Make bitplane to pixel lookup table */
	for(i = 0; i < 0x100; i++)
	for(j = 0; j < 0x100; j++)
	{
		int x;
		uint32 out = 0;
		for(x = 0; x < 8; x++)
		{
			out |= (j & (0x80 >> x)) ? (uint32)(8 << (x << 2)) : 0;
			out |= (i & (0x80 >> x)) ? (uint32)(4 << (x << 2)) : 0;
		}
#if LSB_FIRST
		bp_lut[(j << 8) | (i)] = out;
#else
		bp_lut[(i << 8) | (j)] = out;
#endif
	}
}

void make_tms_tables(void)
{
	int i, j, x;
	int bd, pg, ct;
	int sx, bx;

	for(sx = 0; sx < 16; sx++)
	{
		for(bx = 0; bx < 256; bx++)
		{
//          uint8 bd = (bx & 0x0F);
			uint8 bs = (bx & 0x40);
//          uint8 bt = (bd == 0) ? 1 : 0;
			uint8 sd = (sx & 0x0F);
//          uint8 st = (sd == 0) ? 1 : 0;

			// opaque sprite pixel, choose 2nd pal and set sprite marker
			if(sd && !bs)
			{
				tms_obj_lut[(sx<<8)|(bx)] = sd | 0x10 | 0x40;
			}
			else
			if(sd && bs)
			{
				// writing over a sprite
				tms_obj_lut[(sx<<8)|(bx)] = bx;
			}
			else
			{
				tms_obj_lut[(sx<<8)|(bx)] = bx;
			}
		}
	}


	/* Text lookup table */
	for(bd = 0; bd < 256; bd++)
	{
			uint8 bg = (bd >> 0) & 0x0F;
			uint8 fg = (bd >> 4) & 0x0F;

			/* If foreground is transparent, use background color */
			if(fg == 0) fg = bg;

			txt_lookup[bd][0] = bg;
			txt_lookup[bd][1] = fg;
	}

	/* Multicolor lookup table */
	for(bd = 0; bd < 16; bd++)
	{
		for(pg = 0; pg < 256; pg++)
		{
			int l = (pg >> 4) & 0x0F;
			int r = (pg >> 0) & 0x0F;

			/* If foreground is transparent, use background color */
			if(l == 0) l = bd;
			if(r == 0) r = bd;

			/* Unpack 2 nibbles across eight pixels */
			for(x = 0; x < 8; x++)
			{
				int c = (x & 4) ? r : l;

				mc_lookup[bd][pg][x] = c;
			}
		}
	}

	/* Make bitmap data expansion table */
	memset(bp_expand, 0, sizeof(bp_expand));
	for(i = 0; i < 256; i++)
	{
		for(j = 0; j < 8; j++)
		{
			int c = (i >> (j ^ 7)) & 1;
			bp_expand[i][j] = c;
		}
	}

	/* Graphics I/II lookup table */
	for(bd = 0; bd < 0x10; bd++)
	{
		for(ct = 0; ct < 0x100; ct++)
		{
			int backdrop = (bd & 0x0F);
			int background = (ct >> 0) & 0x0F;
			int foreground = (ct >> 4) & 0x0F;

			/* If foreground is transparent, use background color */
			if(background == 0) background = backdrop;
			if(foreground == 0) foreground = backdrop;

			tms_lookup[bd][ct][0] = background;
			tms_lookup[bd][ct][1] = foreground;
		}
	}
}

int main() {
	make_render_luts();
	make_tms_tables();

	FILE* hf = fopen("lut.h", "w");
	FILE* f = fopen("lut.c", "w");

	fprintf(hf, "#pragma once\n\n");
	fprintf(hf, "#include \"types.h\"\n\n");
	fprintf(f, "#include \"types.h\"\n\n");

	fprintf(hf, "extern const uint8 pixel_lut[];\n");
	fprintf(f, "const uint8 pixel_lut[] = {");
	for (int i = 0; i < 0x10000; i++) {
		if (i % 16 == 0) fprintf(f, "\n\t");
		fprintf(f, "0x%x, ", lut[i]);
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint32 bp_lut[];\n");
	fprintf(f, "const uint32 bp_lut[] = {");
	for (int i = 0; i < 0x10000; i++) {
		if (i % 8 == 0) fprintf(f, "\n\t");
		fprintf(f, "0x%08x, ", bp_lut[i]);
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint8 tms_lookup[16][256][2];\n");
	fprintf(f, "const uint8 tms_lookup[16][256][2] = {\n");
	for (int k = 0; k < 16; k++) {
		fprintf(f, "{");
		for (int j = 0; j < 256; j++) {
			if (j % 8 == 0) fprintf(f, "\n\t");
			fprintf(f, "{0x%x, 0x%x}, ", tms_lookup[k][j][0], tms_lookup[k][j][1]);
		}
		fprintf(f, "\n},\n");
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint8 mc_lookup[16][256][8];\n");
	fprintf(f, "const uint8 mc_lookup[16][256][8] = {\n");
	for (int k = 0; k < 16; k++) {
		fprintf(f, "{");
		for (int j = 0; j < 256; j++) {
			if (j % 2 == 0) fprintf(f, "\n\t");
			fprintf(f, "{");
			for (int i = 0; i < 8; i++) {
				fprintf(f, "0x%x", mc_lookup[k][j][i]);
				if (i < 7) fprintf(f, ", ");
			}
			fprintf(f, "}, ");
		}
		fprintf(f, "\n},\n");
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint8 txt_lookup[256][2];\n");
	fprintf(f, "const uint8 txt_lookup[256][2] = {");
	for (int j = 0; j < 256; j++) {
		if (j % 8 == 0) fprintf(f, "\n\t");
		fprintf(f, "{0x%x, 0x%x}, ", txt_lookup[j][0], txt_lookup[j][1]);
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint8 bp_expand[256][8];\n");
	fprintf(f, "const uint8 bp_expand[256][8] = {");
	for (int j = 0; j < 256; j++) {
		if (j % 2 == 0) fprintf(f, "\n\t");
		fprintf(f, "{");
		for (int i = 0; i < 8; i++) {
			fprintf(f, "0x%x", bp_expand[j][i]);
			if (i < 7) fprintf(f, ", ");
		}
		fprintf(f, "}, ");
	}
	fprintf(f, "\n};\n\n");

	fprintf(hf, "extern const uint8 tms_obj_lut[];\n");
	fprintf(f, "const uint8 tms_obj_lut[] = {");
	for (int i = 0; i < 16*256; i++) {
		if (i % 16 == 0) fprintf(f, "\n\t");
		fprintf(f, "0x%x, ", tms_obj_lut[i]);
	}
	fprintf(f, "\n};\n\n");

	fclose(hf);
	fclose(f);

	return 0;
}