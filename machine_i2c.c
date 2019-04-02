/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Hiroaki OHSAWA
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

#include "i2c.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "modmachine.h"

const mp_obj_base_t machine_i2c_obj_template = {&machine_i2c_type};

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }

#define I2C_DEV "I2C_0"
#define I2C_DEV_0 DT_I2C_0_NAME
#define I2C_DEV_1 DT_I2C_1_NAME
#include <stdio.h>

u32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_FAST);

void pyb_buf_get_for_send(mp_obj_t o, mp_buffer_info_t *bufinfo, byte *tmp_data) {
	if (MP_OBJ_IS_INT(o)) {
		tmp_data[0] = mp_obj_get_int(o);
		bufinfo->buf = tmp_data;
		bufinfo->len = 1;
		bufinfo->typecode = 'B';
	} else {
		mp_get_buffer_raise(o, bufinfo, MP_BUFFER_READ);
	}
}

STATIC void pyb_i2c_read_into (struct device *dev, mp_arg_val_t *args, vstr_t *vstr) {
	//TODO: check init
	// get the buffer to receive into
	if (MP_OBJ_IS_INT(args[1].u_obj)) {
		// allocate a new bytearray of given length
		vstr_init_len(vstr, mp_obj_get_int(args[1].u_obj));
	} else {
		// get the existing buffer
		mp_buffer_info_t bufinfo;
		mp_get_buffer_raise(args[1].u_obj, &bufinfo, MP_BUFFER_WRITE);
		vstr->buf = bufinfo.buf;
		vstr->len = bufinfo.len;
	}
	// receive the data

	//TODO:check error
	i2c_read(dev, (byte *)vstr->buf, vstr->len, args[0].u_int);
}

mp_obj_t machine_i2c_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
	// parse args
	enum { ARG_id, ARG_scl, ARG_sda, ARG_freq, ARG_timeout };
	static const mp_arg_t allowed_args[] = {
		{ MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ },
		{ MP_QSTR_scl, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
		{ MP_QSTR_sda, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
		{ MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_INT },
		{ MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT },
	};
	mp_arg_val_t pargs[MP_ARRAY_SIZE(allowed_args)];
	mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, pargs);

	// work out i2c bus
	machine_i2c_obj_t *self = m_new_obj(machine_i2c_obj_t);
	int i2c_id = mp_obj_get_int(args[ARG_id]);

	switch (i2c_id){
	case 0:
		self->dev = device_get_binding(I2C_DEV_0);
		break;
	case 1:
		self->dev = device_get_binding(I2C_DEV_1);
		break;
	default:
		mp_raise_ValueError("device not found");
		break;
	}
	if (self->dev == NULL) {
		mp_raise_ValueError("device not found");
	}

	self->base = machine_i2c_obj_template;
	self->i2c_channel = i2c_id;
	RAISE_ERRNO(i2c_configure(self->dev, i2c_cfg));
	return (mp_obj_t)self;
}

STATIC void machine_i2c_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
	machine_i2c_obj_t *self = self_in;
	mp_printf(print, "I2C%u", self->i2c_channel);
}
#include <stdio.h>

STATIC mp_obj_t machine_i2c_scan(mp_obj_t self_in) {
	machine_i2c_obj_t *self = self_in;
	mp_obj_t list = mp_obj_new_list(0, NULL);
	uint8_t addr = 0x0;
	char buf;

	for (addr = 0x0; addr <= 0x77; addr++) {
		if (!i2c_read(self->dev, &buf, 0, addr)) {
			mp_obj_list_append(list, mp_obj_new_int(addr));
		}
	}
	return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_scan_obj, machine_i2c_scan);

STATIC mp_obj_t machine_i2c_readfrom(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
	machine_i2c_obj_t *self = pos_args[0];
	STATIC const mp_arg_t pyb_i2c_readfrom_args[] = {
		{ MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, },
		{ MP_QSTR_nbytes,  MP_ARG_REQUIRED | MP_ARG_OBJ, },
	};
	// parse args
	mp_arg_val_t args[MP_ARRAY_SIZE(pyb_i2c_readfrom_args)];
	mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), pyb_i2c_readfrom_args, args);

	vstr_t vstr;
	pyb_i2c_read_into(self->dev, args, &vstr);
	// return the received data
	return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_obj, 1, machine_i2c_readfrom);

STATIC mp_obj_t machine_i2c_writeto(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
	machine_i2c_obj_t *self = pos_args[0];
	mp_buffer_info_t bufinfo;
	uint8_t data[1];

	STATIC const mp_arg_t pyb_i2c_writeto_args[] = {
		{ MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, },
		{ MP_QSTR_buf,  MP_ARG_REQUIRED | MP_ARG_OBJ, },
		{ MP_QSTR_stop,    MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(pyb_i2c_writeto_args)];
	mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), pyb_i2c_writeto_args, args);

	pyb_buf_get_for_send(args[1].u_obj, &bufinfo, data);
	if (i2c_write(self->dev, bufinfo.buf, bufinfo.len, args[0].u_int) < 0) {
		mp_raise_ValueError("write failure");
	}

	return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_writeto_obj, 1, machine_i2c_writeto);


STATIC const mp_rom_map_elem_t machine_i2c_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&machine_i2c_scan_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readfrom), MP_ROM_PTR(&machine_i2c_readfrom_obj) },
	{ MP_ROM_QSTR(MP_QSTR_writeto), MP_ROM_PTR(&machine_i2c_writeto_obj) },
};

STATIC MP_DEFINE_CONST_DICT(machine_i2c_locals_dict, machine_i2c_locals_dict_table);

const mp_obj_type_t machine_i2c_type = {
	{ &mp_type_type },
	.name = MP_QSTR_I2C,
	.print = machine_i2c_print,
	.make_new = machine_i2c_make_new,
	.locals_dict = (mp_obj_t)&machine_i2c_locals_dict,
};
