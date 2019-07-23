/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include "uart.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "modmachine.h"

const mp_obj_base_t machine_uart_obj_template = {&machine_uart_type};

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }
#define UART_DEV_0 DT_UART_0_NAME
#define UART_0 0

enum {ARG_baudrate, ARG_bits, ARG_parity, ARG_stop };
STATIC const char *_parity_name[] = {"None","ODD","Even"};

#define BUF_SIZE 4096
u8_t uart_rxbuffer[BUF_SIZE];
volatile int uart_rxbuf_write_cursor = 0;
volatile int uart_rxbuf_read_cursor = 0;
u8_t uart_txbuffer[BUF_SIZE];
volatile int uart_txbuf_write_cursor = 0;
volatile int uart_txbuf_read_cursor = 0;

static void uart_cb(struct device *dev){
	uart_irq_update(dev);
	if(uart_irq_rx_ready(dev)){
		uart_fifo_read(dev, (u8_t *)&uart_rxbuffer[uart_rxbuf_write_cursor], 1);
		uart_rxbuf_write_cursor = (uart_rxbuf_write_cursor + 1) % BUF_SIZE;
		if(uart_rxbuf_write_cursor == uart_rxbuf_read_cursor){
			uart_rxbuf_read_cursor = (uart_rxbuf_read_cursor + 1) % BUF_SIZE;
		}
	}

	if(uart_irq_tx_ready(dev) && (uart_txbuf_read_cursor != uart_txbuf_write_cursor)){		
		uart_fifo_fill(dev, (u8_t *)&uart_txbuffer[uart_txbuf_read_cursor], 1);
		uart_txbuf_read_cursor = (uart_txbuf_read_cursor + 1) % BUF_SIZE;
		if(uart_txbuf_read_cursor == uart_txbuf_write_cursor){
			while(1){
				if(uart_irq_tx_complete(dev)){
					uart_irq_tx_disable(dev);
					break;
				}
			}
		}
	}
}

static int uart_read_bytes(u8_t* data, int size){
	int read_size = 0;

	unsigned int key;
	while(1){
		if(uart_rxbuf_read_cursor != uart_rxbuf_write_cursor){
			key = irq_lock();
			*data = uart_rxbuffer[uart_rxbuf_read_cursor];
			uart_rxbuf_read_cursor = (uart_rxbuf_read_cursor + 1) % BUF_SIZE;
			read_size ++;
			data ++;
			irq_unlock(key);
		}
		else{
			;
		}

		if(size <= read_size){
			break;
		}
	}
	return read_size;
}

static int uart_write_bytes(const u8_t* data, int size){
	int write_size = 0;
	for(write_size=0; write_size < size; write_size++){
		uart_txbuffer[uart_txbuf_write_cursor] = *data;
		uart_txbuf_write_cursor = (uart_txbuf_write_cursor + 1) % BUF_SIZE;
		data++;
	 	if(uart_txbuf_write_cursor == uart_txbuf_read_cursor){
			break;
		}
	}
	return write_size;
}

STATIC void machine_uart_init_helper(machine_uart_obj_t *self, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args){
	static const mp_arg_t allowed_args[] = {
		{ MP_QSTR_baudrate, MP_ARG_INT, {.u_int = 0} },
		{ MP_QSTR_bits, MP_ARG_INT, {.u_int = 0} },
		{ MP_QSTR_parity, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
		{ MP_QSTR_stop, MP_ARG_INT , {.u_int = 0} }
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

	while(1){
		if(uart_irq_tx_complete(self->dev)){
			break;
		}
	}

	struct uart_config cfg;
	RAISE_ERRNO(uart_config_get(self->dev, &cfg));

	if(args[ARG_baudrate].u_int > 0){
		switch(args[ARG_baudrate].u_int){
			case 1200:
			case 2400:
			case 4800:
			case 9600:
			case 14400:
			case 19200:
			case 28800:
			case 31250:
			case 38400:
			case 56000:
			case 57600:
			case 76800:
			case 115200:
			case 230400:
			case 250000:
			case 460800:
			case 921600:
			case 1000000:
				self->cfg.baudrate = args[ARG_baudrate].u_int;
				break;
			default :
				mp_raise_ValueError("invalid baudrate");
				break;
		}
	}
	else{
		self->cfg.baudrate = cfg.baudrate;
	}

	if(args[ARG_parity].u_obj != MP_OBJ_NULL){
		if(args[ARG_parity].u_obj == mp_const_none){
			self->cfg.parity = UART_CFG_PARITY_NONE;
		}
		else if(mp_obj_get_int(args[ARG_parity].u_obj) == 0){
			self->cfg.parity = UART_CFG_PARITY_EVEN;
		}
		else{
			mp_raise_ValueError("invalid parity");
		}
	}
	else{
		self->cfg.parity = cfg.parity;
	}

	if(args[ARG_stop].u_int != 0){		
		if(args[ARG_stop].u_int == 1){
			self->cfg.stop_bits = UART_CFG_STOP_BITS_1;
		}
		else{
			mp_raise_ValueError("invalid stop bits");
		}
	}
	else{
		self->cfg.stop_bits = cfg.stop_bits;
	}

	if(args[ARG_bits].u_int != 0){
		if(args[ARG_bits].u_int == 8){
			self->cfg.data_bits = UART_CFG_DATA_BITS_8;
		}
		else{
			mp_raise_ValueError("invalid data bits");
		}
	}
	else{
		self->cfg.data_bits = cfg.data_bits;
	}
	self->cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

	RAISE_ERRNO(uart_configure(self->dev, &self->cfg));
}

STATIC mp_obj_t machine_uart_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args){
	mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);

	machine_uart_obj_t *self = m_new_obj(machine_uart_obj_t);
	self->base = machine_uart_obj_template;

	int uart_id = mp_obj_get_int(args[0]);
	if(uart_id == UART_0){
		self->dev = device_get_binding(UART_DEV_0);
		self->uart_channel = uart_id;
	}
	else{
		mp_raise_ValueError("device not found");
	}

	mp_map_t kw_args;
    	mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    	machine_uart_init_helper(self, n_args - 1, args + 1, &kw_args);

	uart_irq_callback_set(self->dev, uart_cb);
	uart_irq_rx_enable(self->dev);

	return (mp_obj_t)self;
}

STATIC mp_obj_t machine_uart_init(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    machine_uart_init_helper(args[0], n_args - 1, args + 1, kw_args);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_uart_init_obj, 1, machine_uart_init);

STATIC void machine_uart_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
	machine_uart_obj_t *self = self_in;
	mp_printf(print, "UART(%u, baudrate=%u, bits=%u, parity=%s, stop=%u)",
		self->uart_channel, self->cfg.baudrate, self->cfg.data_bits, _parity_name[self->cfg.parity], self->cfg.stop_bits);
}

STATIC mp_obj_t machine_uart_deinit(mp_obj_t self_in){
	machine_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
	while(1){
		if(uart_irq_tx_complete(self->dev)){
			uart_irq_tx_disable(self->dev);
			break;
		}
	}
	uart_irq_rx_disable(self->dev);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_uart_deinit_obj, machine_uart_deinit);

STATIC mp_obj_t machine_uart_any(mp_obj_t self_in){
	size_t rxbufsize = 0;
	
	if(uart_rxbuf_write_cursor > uart_rxbuf_read_cursor){
		rxbufsize = uart_rxbuf_write_cursor - uart_rxbuf_read_cursor;
	}
	else if(uart_rxbuf_write_cursor < uart_rxbuf_read_cursor){
		rxbufsize = BUF_SIZE - (uart_rxbuf_read_cursor - uart_rxbuf_write_cursor);
	}
	return MP_OBJ_NEW_SMALL_INT(rxbufsize);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_uart_any_obj, machine_uart_any);

STATIC const mp_rom_map_elem_t machine_uart_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_uart_init_obj) },
	{ MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&machine_uart_deinit_obj) },
	{ MP_ROM_QSTR(MP_QSTR_any), MP_ROM_PTR(&machine_uart_any_obj) },
	{ MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
	{ MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};

STATIC MP_DEFINE_CONST_DICT(machine_uart_locals_dict, machine_uart_locals_dict_table);

STATIC mp_uint_t machine_uart_read(mp_obj_t self_in, void *buf_in, mp_uint_t size, int *errcode) {
	int read_size = 0;
	if(size > 0){
		read_size = uart_read_bytes(buf_in, size);
		if(read_size == 0){
			*errcode = MP_EAGAIN;
			read_size = MP_STREAM_ERROR;
		}
	}
	return read_size;
}

STATIC mp_uint_t machine_uart_write(mp_obj_t self_in, const void *buf_in, mp_uint_t size, int *errcode) {
	machine_uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
	int write_size = 0;
	if(size > 0){
		write_size = uart_write_bytes(buf_in, size);
		uart_irq_tx_enable(self->dev);
	}
	return write_size;
}

STATIC const mp_stream_p_t uart_stream_p = {
	.read = machine_uart_read,
	.write = machine_uart_write,
	.ioctl = false,
	.is_text = false,
};

const mp_obj_type_t machine_uart_type = {
	{ &mp_type_type },
	.name = MP_QSTR_UART,
	.print = machine_uart_print,
	.make_new = machine_uart_make_new,
	.protocol = &uart_stream_p,
	.locals_dict = (mp_obj_t)&machine_uart_locals_dict,
};

