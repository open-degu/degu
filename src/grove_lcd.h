/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>

#define SLEEP_IN_US(_x_)	((_x_) * 1000)
#define GROVE_LCD_DISPLAY_ADDR		(0x3E)
#define GROVE_RGB_BACKLIGHT_ADDR	(0x62)

#define GROVE_LCD_DISPLAY_ONELINE_MAX 16

struct command {
	u8_t control;
	u8_t data;
};

struct glcd_data {
	struct device *i2c;
	u8_t input_set;
	u8_t display_switch;
	u8_t function;
};

struct glcd_driver {
	u16_t	lcd_addr;
	u16_t	rgb_addr;
};

#define ON	0x1
#define OFF	0x0

/********************************************
 *  LCD FUNCTIONS
 *******************************************/
/* GLCD_CMD_SCREEN_CLEAR has no options */
/* GLCD_CMD_CURSOR_RETURN has no options */

/* Defines for the GLCD_CMD_CURSOR_SHIFT */
#define GLCD_CS_DISPLAY_SHIFT		(1 << 3)
#define GLCD_CS_RIGHT_SHIFT		(1 << 2)

/* LCD Display Commands */
#define GLCD_CMD_SCREEN_CLEAR		(1 << 0)
#define GLCD_CMD_CURSOR_RETURN		(1 << 1)
#define GLCD_CMD_INPUT_SET		(1 << 2)
#define GLCD_CMD_DISPLAY_SWITCH		(1 << 3)
#define GLCD_CMD_CURSOR_SHIFT		(1 << 4)
#define GLCD_CMD_FUNCTION_SET		(1 << 5)
#define GLCD_CMD_SET_CGRAM_ADDR		(1 << 6)
#define GLCD_CMD_SET_DDRAM_ADDR		(1 << 7)

/********************************************
 *  RGB FUNCTIONS
 *******************************************/
#define REGISTER_POWER  0x08
#define REGISTER_R	0x04
#define REGISTER_G	0x03
#define REGISTER_B	0x02

static u8_t color_define[][3] = {
	{ 255, 255, 255 },	/* white */
	{ 255, 0,   0   },      /* red */
	{ 0,   255, 0   },      /* green */
	{ 0,   0,   255 },      /* blue */
};

/********************************************
 *  PUBLIC FUNCTIONS
 *******************************************/
void glcd_print(struct device *port, char *data, u32_t size);
void glcd_cursor_pos_set(struct device *port, u8_t col, u8_t row);
void glcd_clear(struct device *port);
void glcd_display_state_set(struct device *port, u8_t opt);
u8_t glcd_display_state_get(struct device *port);
void glcd_input_state_set(struct device *port, u8_t opt);
u8_t glcd_input_state_get(struct device *port);
void glcd_color_select(struct device *port, u8_t color);
void glcd_color_set(struct device *port, u8_t r, u8_t g, u8_t b);
void glcd_function_set(struct device *port, u8_t opt);
u8_t glcd_function_get(struct device *port);
int glcd_initialize(struct device *port);