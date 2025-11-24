
#ifndef _RENDER_H_
#define _RENDER_H_

/* Pack RGB data into a 16-bit RGB 5:6:5 format */
#define MAKE_PIXEL(r,g,b)   (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F))

/* Used for blanking a line in whole or in part */
#define BACKDROP_COLOR      (0x10 | (vdp.reg[7] & 0x0F))

#define LINEBUF_COUNT 8

extern uint8 sms_cram_expand_table[4];
extern uint8 gg_cram_expand_table[16];
extern void (*render_bg)(int line);
extern void (*render_obj)(int line);
extern uint8 *linebuf;
extern uint8 internal_buffer[LINEBUF_COUNT][0x100];
extern uint16 pixel[];
extern uint8 bg_name_dirty[0x200];     
extern uint16 bg_name_list[0x200];     
extern uint16 bg_list_index;           
extern uint8 bg_pattern_cache[0x20000];

void render_shutdown(void);
void render_init(void);
void render_reset(void);
void render_line(int line);
void render_bg_sms(int line);
void render_obj_sms(int line);
void update_bg_pattern_cache(void);
void palette_sync(int index, int force);

#endif /* _RENDER_H_ */
