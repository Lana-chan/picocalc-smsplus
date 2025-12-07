#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "st7789_lcd.pio.h"

#include "lcd.h"
#include "lcd_lut.h"
#include "default_font.h"

#define LCD_SCK 10
#define LCD_TX  11
#define LCD_RX  12
#define LCD_CS  13
#define LCD_DC  14
#define LCD_RST 15
#define LCD_PIO pio1
#define SERIAL_CLK_DIV 1.5f

void(*lcd_draw_ptr) (uint16_t*,int,int,int,int);
void(*lcd_paletted_draw_ptr) (uint8_t*,uint16_t*,int,int,int,int, bool);
void(*lcd_fill_ptr) (uint16_t,int,int,int,int);
void(*lcd_point_ptr) (uint16_t,int,int);
void(*lcd_clear_ptr) (void);

uint lcd_sm = 0;
uint lcd_offset;

uint8_t* framebuffer;
int framebuffer_mode;

#define LCD_TMPBUF_SIZE LCD_WIDTH*2
uint16_t lcd_tmpbuf[LCD_TMPBUF_SIZE];

static inline void __not_in_flash_func(lcd_set_dc_cs)(bool dc, bool cs) {
	gpio_put_masked((1u << LCD_DC) | (1u << LCD_CS), !!dc << LCD_DC | !!cs << LCD_CS);
}

static inline void lcd_write_cmd(const uint8_t *cmd, size_t count) {
	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 0);
	st7789_lcd_put(LCD_PIO, lcd_sm, *cmd++);
	if (count >= 2) {
		st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
		lcd_set_dc_cs(1, 0);
		for (size_t i = 0; i < count - 1; ++i)
			st7789_lcd_put(LCD_PIO, lcd_sm, *cmd++);
	}
	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 1);
}

static inline void lcd_write16(const uint16_t *data, size_t count) {
	uint16_t color;
	for (size_t i = 0; i < count; ++i) {
		color = *data++;
		st7789_lcd_put(LCD_PIO, lcd_sm, color >> 8);
		st7789_lcd_put(LCD_PIO, lcd_sm, color & 0xff);
	}
}

static inline void lcd_initcmd(const uint8_t *init_seq) {
	const uint8_t *cmd = init_seq;
	while (*cmd) {
		lcd_write_cmd(cmd + 2, *cmd);
		sleep_ms(*(cmd + 1) * 5);
		cmd += *cmd + 2;
	}
}

static void __not_in_flash_func(lcd_set_region)(int x1, int y1, int x2, int y2) {
	lcd_set_dc_cs(0, 0);
	const uint8_t cmd1[] = {0x2A, (x1 >> 8), (x1 & 0xFF), (x2 >> 8), (x2 & 0xFF)};
	const uint8_t cmd2[] = {0x2B, (y1 >> 8), (y1 & 0xFF), (y2 >> 8), (y2 & 0xFF)};
	lcd_write_cmd(cmd1, 5);
	lcd_write_cmd(cmd2, 5);
	uint8_t cmd = 0x2c; // RAMWR
	lcd_write_cmd(&cmd, 1);
	busy_wait_us(1);
	lcd_set_dc_cs(1, 0);
}

void lcd_blank() {
	// Enter Sleep
	uint8_t cmd = 0x10;
	lcd_write_cmd(&cmd, 1);
}

void lcd_unblank() {
	// Exit Sleep
	uint8_t cmd = 0x11;
	lcd_write_cmd(&cmd, 1);
}

void lcd_on() {
	// Display on
	uint8_t cmd = 0x29;
	lcd_write_cmd(&cmd, 1);
}

void lcd_off() {
	// Display off
	uint8_t cmd = 0x29;
	lcd_write_cmd(&cmd, 1);
}

static inline void normalize_coords(int* x, int* y, int* width, int* height, int scrheight) {
	*y %= scrheight;
	*x = (*x < 0 ? 0 : (*x >= LCD_WIDTH ? LCD_WIDTH : *x));
	*width = (*width < 0 ? 0 : (*x + *width >= LCD_WIDTH ? LCD_WIDTH - *x : *width));
	*height = (*height < 0 ? 0 : (*y + *height >= scrheight ? scrheight - *y : *height));
}

static void lcd_direct_draw(uint16_t* pixels, int x, int y, int width, int height) {
	normalize_coords(&x, &y, &width, &height, MEM_HEIGHT);
	lcd_set_region(x, y, x + width - 1, y + height - 1);

	lcd_write16(pixels, width * height);

	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 1);
}

static void __not_in_flash_func(lcd_direct_paletted_draw)(uint8_t* pixels, uint16_t* palette, int x, int y, int width, int height, bool dbl) {
	int lcd_width = width * (dbl ? 2 : 1);
	int lcd_height = height * (dbl ? 2 : 1);
	normalize_coords(&x, &y, &lcd_width, &lcd_height, MEM_HEIGHT);
	lcd_set_region(x, y, x + lcd_width - 1, y + lcd_height - 1);

	uint8_t* pixels_ptr = pixels;

	if (dbl) {
		for (size_t i = 0; i < width * height; ++i) {
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] >> 8);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] & 0xff);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] >> 8);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr++] & 0xff);
		}
		pixels_ptr = pixels;
		for (size_t i = 0; i < width * height; ++i) {
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] >> 8);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] & 0xff);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] >> 8);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr++] & 0xff);
		}
	} else {
		for (size_t i = 0; i < width * height; ++i) {
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr] >> 8);
			st7789_lcd_put(LCD_PIO, lcd_sm, palette[*pixels_ptr++] & 0xff);
		}
	}

	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 1);
}

static void __not_in_flash_func(lcd_direct_fill)(uint16_t color, int x, int y, int width, int height) {
	normalize_coords(&x, &y, &width, &height, MEM_HEIGHT);
	lcd_set_region(x, y, x + width - 1, y + height - 1);
	
	uint8_t c1 = color >> 8;
	uint8_t c2 = color & 0xff;

	for (size_t i = 0; i < width * height; ++i) {
		st7789_lcd_put(LCD_PIO, lcd_sm, c1);
		st7789_lcd_put(LCD_PIO, lcd_sm, c2);
	}

	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 1);
}

static void lcd_direct_point(uint16_t color, int x, int y) {
	lcd_direct_fill(color, x, y, 1, 1);
}

static void lcd_direct_clear() {
	lcd_direct_fill(0, 0, 0, LCD_WIDTH, MEM_HEIGHT);
}

static void lcd_ram_draw(uint16_t* pixels, int x, int y, int width, int height) {
	normalize_coords(&x, &y, &width, &height, LCD_HEIGHT);

	for (uint32_t iy = y * LCD_WIDTH; iy < (y + height) * LCD_WIDTH; iy += LCD_WIDTH) {
		for (uint32_t ix = x; ix < (x + width); ix++) {
			framebuffer[iy+ix] = lcd_to8[*pixels++];
		}
	}
}

static void lcd_ram_paletted_draw(uint8_t* pixels, uint16_t* palette, int x, int y, int width, int height, bool dbl) {
	normalize_coords(&x, &y, &width, &height, LCD_HEIGHT);

	for (uint32_t iy = y * LCD_WIDTH; iy < (y + height) * LCD_WIDTH; iy += LCD_WIDTH) {
		for (uint32_t ix = x; ix < (x + width); ix++) {
			// TODO: double
			framebuffer[iy+ix] = lcd_to8[palette[*pixels++]];
		}
	}
}

static void lcd_ram_fill(uint16_t color, int x, int y, int width, int height) {
	normalize_coords(&x, &y, &width, &height, LCD_HEIGHT);

	for (uint32_t iy = y * LCD_WIDTH; iy < (y + height) * LCD_WIDTH; iy += LCD_WIDTH) {
		memset(framebuffer + x + iy, lcd_to8[color], width);
	}
}

static void lcd_ram_point(uint16_t color, int x, int y) {
	if (x >= 0 && y >= 0 && x < LCD_WIDTH && y < LCD_HEIGHT)
		framebuffer[(x + y * LCD_WIDTH)] = lcd_to8[color];
}

static void lcd_ram_clear() {
	memset(framebuffer, 0, LCD_WIDTH * LCD_HEIGHT);
}

void lcd_buffer_blit_local() {
	if (framebuffer_mode == LCD_BUFFERMODE_DIRECT) return;
	
	lcd_set_region(0, 0, 319, 319);

	uint16_t color;
	for (size_t count = 0; count < LCD_WIDTH * LCD_HEIGHT; count++) {
		color = lcd_to16[framebuffer[count]];
		st7789_lcd_put(LCD_PIO, lcd_sm, color >> 8);
		st7789_lcd_put(LCD_PIO, lcd_sm, color & 0xff);
	}

	st7789_lcd_wait_idle(LCD_PIO, lcd_sm);
	lcd_set_dc_cs(0, 1);
}

void lcd_draw_local(uint16_t* pixels, int x, int y, int width, int height) {
	lcd_draw_ptr(pixels, x, y, width, height);
}

void inline lcd_paletted_draw_local(uint8_t* pixels, uint16_t* palette, int x, int y, int width, int height, bool dbl) {
	lcd_paletted_draw_ptr(pixels, palette, x, y, width, height, dbl);
}

void lcd_fill_local(uint16_t color, int x, int y, int width, int height) {
	lcd_fill_ptr(color, x, y, width, height);
}

void lcd_point_local(uint16_t color, int x, int y) {
	lcd_point_ptr(color, x, y);
}

void lcd_clear_local() {
	lcd_clear_ptr();
}

bool lcd_buffer_enable_local(int mode) {
	if (mode != LCD_BUFFERMODE_RAM) {
		if (framebuffer) {
			free(framebuffer);
			framebuffer = NULL;
		}
	}

	if (mode == LCD_BUFFERMODE_DIRECT) {
		lcd_draw_ptr = &lcd_direct_draw;
		lcd_paletted_draw_ptr = &lcd_direct_paletted_draw;
		lcd_fill_ptr = &lcd_direct_fill;
		lcd_point_ptr = &lcd_direct_point;
		lcd_clear_ptr = &lcd_direct_clear;
		framebuffer_mode = mode;
		return true;
	} else if (mode == LCD_BUFFERMODE_RAM) {
		if (!framebuffer) {
			framebuffer = malloc(LCD_WIDTH * LCD_HEIGHT);
			if (framebuffer) {
				lcd_draw_ptr = &lcd_ram_draw;
				lcd_paletted_draw_ptr = &lcd_ram_paletted_draw;
				lcd_fill_ptr = &lcd_ram_fill;
				lcd_point_ptr = &lcd_ram_point;
				lcd_clear_ptr = &lcd_ram_clear;
				framebuffer_mode = mode;
				return true;
			}
		}
	}

	return false;
}

void lcd_scroll_local(int lines) {
	lines %= MEM_HEIGHT;
	uint8_t cmd[] = {0x37, (lines >> 8), (lines & 0xFF)};
	lcd_write_cmd(cmd, 3);
}

void lcd_setup_scrolling(int top_fixed_lines, int bottom_fixed_lines) {
	int vertical_scrolling_area = LCD_HEIGHT - (top_fixed_lines + bottom_fixed_lines);
	const uint8_t cmd[] = {0x33,
		(top_fixed_lines >> 8), (top_fixed_lines & 0xFF),
		(vertical_scrolling_area >> 8), (vertical_scrolling_area & 0xFF),
		(bottom_fixed_lines >> 8), (bottom_fixed_lines & 0xff)
	};
	lcd_write_cmd(cmd, 7);
}

font_t font = {
	.glyphs = NULL,
	.glyph_width = 0,
	.glyph_height = 0,
	.glyph_colorbuf = NULL
};

void lcd_load_font() {
	if (font.glyphs) { free(font.glyphs); font.glyphs = NULL; }
	if (font.glyph_colorbuf) { free(font.glyph_colorbuf); font.glyph_colorbuf = NULL; }

	font.glyphs = malloc(2049 * sizeof(uint8_t));
	memcpy(font.glyphs, (uint8_t*)DEFAULT_GLYPHS, 2049);
	font.glyph_count = 255;
	font.glyph_width = DEFAULT_GLYPH_WIDTH;
	font.glyph_height = DEFAULT_GLYPH_HEIGHT;
	font.firstcode = 0;

	font.bytewidth = font.glyph_width/8 + (font.glyph_width % 8 != 0);
	font.glyph_colorbuf = malloc(font.glyph_height * font.glyph_width * sizeof(uint16_t));
	font.term_width = LCD_WIDTH / font.glyph_width;
	font.term_height = LCD_HEIGHT / font.glyph_height;
}

void lcd_draw_char_local(int x, int y, uint16_t fg, uint16_t bg, char c) {
	if (c > font.glyph_count + font.firstcode) c = 0;
	int offset = ((uint8_t)(c - font.firstcode)) * font.bytewidth * font.glyph_height;
	for (int j = 0; j < font.glyph_height; j++) {
		for (int i = 0; i < font.glyph_width; i++) {
			int mask = (1 << (7 - i%8));
			font.glyph_colorbuf[i + j * font.glyph_width] = (font.glyphs[offset + i / 8] & mask) ? fg : bg;
		}
		offset+=font.bytewidth;
	}

	lcd_draw(font.glyph_colorbuf, x, y, font.glyph_width, font.glyph_height);
}

void lcd_draw_text_local(int x, int y, uint16_t fg, uint16_t bg, const char* text, size_t len, uint8_t align) {
	//if (y <= -font.glyph_height || y >= HEIGHT) return;
	if (align == LCD_ALIGN_CENTER) x -= len * font.glyph_width / 2;
	else if (align == LCD_ALIGN_RIGHT) x -= len * font.glyph_width;
	for (int i = 0; i < len; i++) {
		lcd_draw_char(x, y, fg, bg, *text);
		x += font.glyph_width;
		if (x > LCD_WIDTH) return;
		text ++;
	}
}

void lcd_printf(int x, int y, uint16_t fg, uint16_t bg, const char* format, ...) {
	char buffer[512];
	va_list list;
	va_start(list, format);
	int result = vsnprintf(buffer, 512, format, list);
	if (result > -1) {
		lcd_draw_text(x, y, fg, bg, buffer, result, LCD_ALIGN_LEFT);
	}
}

static const uint8_t st7789_init_seq[] = {
	// Positive Gamma Control
	16, 0, 0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
	// Negative Gamma Control
	16, 0, 0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
	// Power Control 1
	3, 0, 0xC0, 0x17, 0x15,
	// Power Control 2
	2, 0, 0xC1, 0x41,
	// VCOM Control
	4, 0, 0xC5, 0x00, 0x12, 0x80,
	// Memory Access Control (0x48=BGR, 0x40=RGB)
	2, 0, 0x36, 0x48,
	// Pixel Interface Format  16 bit colour for SPI
	2, 0, 0x3A, 0x55,
	// Interface Mode Control
	2, 0, 0xB0, 0x00,
	// 60Hz
	3, 0, 0xB1, 0xD0, 0x11,
	// Invert colors on
	1, 0, 0x21,
	// Display Inversion Control
	2, 0, 0xB4, 0x02,
	// Display Function Control
	4, 0, 0xB6, 0x02, 0x02, 0x3B,
	// Entry Mode Set
	2, 0, 0xB7, 0xC6,
	2, 0, 0xE9, 0x00,
	// Adjust Control 3
	5, 0, 0xF7, 0xA9, 0x51, 0x2C, 0x82,
	// Exit Sleep
	1, 0, 0x11,
	// Terminate list
	0
};

void lcd_init() {
	// Init GPIO
	gpio_init(LCD_SCK);
	gpio_init(LCD_TX);
	//gpio_init(LCD_RX);
	gpio_init(LCD_CS);
	gpio_init(LCD_DC);
	gpio_init(LCD_RST);

	gpio_set_dir(LCD_SCK, GPIO_OUT);
	gpio_set_dir(LCD_TX, GPIO_OUT);
	//gpio_set_dir(LCD_RX, GPIO_IN);
	gpio_set_dir(LCD_CS, GPIO_OUT);
	gpio_set_dir(LCD_DC, GPIO_OUT);
	gpio_set_dir(LCD_RST, GPIO_OUT);

	// Init PIO
	lcd_offset = pio_add_program(LCD_PIO, &st7789_lcd_program);
	st7789_lcd_program_init(LCD_PIO, lcd_sm, lcd_offset, LCD_TX, LCD_SCK, SERIAL_CLK_DIV);

	lcd_set_dc_cs(0, 1);
	gpio_put(LCD_RST, 1);

	// Reset controller
	gpio_put(LCD_RST, 0);
	busy_wait_us(20); // 20µs reset pulse (10µs minimum)
	gpio_put(LCD_RST, 1);
	busy_wait_us(120000); // 5ms required after reset, but 120ms needed before sleep out command

	// Setup LCD
	lcd_initcmd(st7789_init_seq);
	lcd_set_dc_cs(0, 1);

	lcd_buffer_enable(0);

	lcd_load_font(NULL);

	lcd_clear();
	lcd_on();
}

int __not_in_flash_func(lcd_fifo_receiver)(uint32_t message) {
	uint32_t x, y, fg, bg, width, height, c, dbl;
	char* text;

	switch (message) {
		case FIFO_LCD_POINT:
			fg = multicore_queue_pop();
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			lcd_point_local((uint16_t)fg, (int)x, (int)y);
			return 1;

		case FIFO_LCD_DRAW:
			fg = multicore_queue_pop();
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			width = multicore_queue_pop();
			height = multicore_queue_pop();
			lcd_draw_local((uint16_t*)fg, (int)x, (int)y, (int)width, (int)height);
			return 1;

		case FIFO_LCD_PALDRAW:
			fg = multicore_queue_pop();
			c = multicore_queue_pop();
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			width = multicore_queue_pop();
			height = multicore_queue_pop();
			dbl = multicore_queue_pop();
			lcd_paletted_draw_local((uint8_t*)fg, (uint16_t*)c, (int)x, (int)y, (int)width, (int)height, (bool)dbl);
			return 1;

		case FIFO_LCD_FILL:
			fg = multicore_queue_pop();
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			width = multicore_queue_pop();
			height = multicore_queue_pop();
			lcd_fill_local((uint16_t)fg, (int)x, (int)y, (int)width, (int)height);
			return 1;

		case FIFO_LCD_CLEAR:
			lcd_clear_local();
			return 1;

		case FIFO_LCD_BUFEN:
			x = multicore_queue_pop();
			multicore_queue_push(lcd_buffer_enable_local(x));
			return 1;

		case FIFO_LCD_BUFBLIT:
			lcd_buffer_blit_local();
			return 1;

		case FIFO_LCD_CHAR:
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			fg = multicore_queue_pop();
			bg = multicore_queue_pop();
			c = multicore_queue_pop();
			lcd_draw_char_local((int)x, (int)y, (uint16_t)fg, (uint16_t)bg, (char)c);
			return 1;

		case FIFO_LCD_TEXT:
			x = multicore_queue_pop();
			y = multicore_queue_pop();
			fg = multicore_queue_pop();
			bg = multicore_queue_pop();
			c = multicore_queue_pop();
			width = multicore_fifo_pop_string(&text);
			lcd_draw_text_local((int)x, (int)y, (uint16_t)fg, (uint16_t)bg, text, width, (uint8_t)c);
			free(text);
			return 1;

		case FIFO_LCD_SCROLL:
			height = multicore_queue_pop();
			lcd_scroll_local((int)height);
			return 1;

		default:
			return 0;
	}
}
