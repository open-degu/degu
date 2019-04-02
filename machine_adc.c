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

#include "adc.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "modmachine.h"

static struct adc_sequence_options options = {
	.interval_us = 12,
	.extra_samplings = 0,
};

static struct adc_sequence adc_seq = {
	.options = &options,
};

const mp_obj_base_t machine_adc_obj_template = {&machine_adc_type};

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }

#define ADC_DEV "ADC_0"

mp_obj_t machine_adc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw,
		       const mp_obj_t *args) {
	mp_arg_check_num(n_args, n_kw, 1, 1, true);
	machine_adc_obj_t *self = m_new_obj(machine_adc_obj_t);
	uint8_t adc_id = mp_obj_get_int(args[0]);

	self->dev = device_get_binding(ADC_DEV);
	if (self->dev == NULL) {
		mp_raise_ValueError("device not found");
	}
	self->base = machine_adc_obj_template;
	self->adc_channel = adc_id;
	self->cfg = m_new_obj(struct adc_channel_cfg);
	self->cfg->channel_id = self->adc_channel;
	self->cfg->differential = false;
	self->cfg->gain = ADC_GAIN_1_6;
	self->cfg->reference = ADC_REF_INTERNAL;
	self->cfg->acquisition_time = ADC_ACQ_TIME_DEFAULT;
	self->cfg->input_positive = self->adc_channel + 1;
	RAISE_ERRNO(adc_channel_setup(self->dev, self->cfg));
	return (mp_obj_t)self;
}

STATIC void machine_adc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
	machine_adc_obj_t *self = self_in;
	mp_printf(print, "ADC/A%u", self->adc_channel);
}

#include <stdio.h>
STATIC mp_obj_t machine_adc_read(mp_obj_t self_in) {
	machine_adc_obj_t *self = self_in;
	adc_seq.buffer = &(self->buffer);
	adc_seq.channels = BIT(self->adc_channel);
	adc_seq.resolution = 12;
	adc_seq.buffer_size = sizeof(self->buffer); //self->buffer
	RAISE_ERRNO(adc_read(self->dev, &adc_seq));
	return MP_OBJ_NEW_SMALL_INT((u16_t)self->buffer);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_adc_read_obj, machine_adc_read);

STATIC mp_obj_t machine_adc_gain(mp_obj_t self_in, mp_obj_t gain_in) {
	machine_adc_obj_t *self = self_in;
	self->cfg->gain = MP_OBJ_QSTR_VALUE(gain_in);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(machine_adc_gain_obj, machine_adc_gain);


STATIC const mp_rom_map_elem_t machine_adc_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&machine_adc_read_obj) },
	{ MP_ROM_QSTR(MP_QSTR_gain), MP_ROM_PTR(&machine_adc_gain_obj) },

	{ MP_ROM_QSTR(MP_QSTR_GAIN_1_6), MP_ROM_INT(ADC_GAIN_1_6) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_1_5), MP_ROM_INT(ADC_GAIN_1_5) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_1_4), MP_ROM_INT(ADC_GAIN_1_4) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_1_3), MP_ROM_INT(ADC_GAIN_1_3) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_1_2), MP_ROM_INT(ADC_GAIN_1_2) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_2_3), MP_ROM_INT(ADC_GAIN_2_3) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_1), MP_ROM_INT(ADC_GAIN_1) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_2), MP_ROM_INT(ADC_GAIN_2) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_3), MP_ROM_INT(ADC_GAIN_3) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_4), MP_ROM_INT(ADC_GAIN_4) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_8), MP_ROM_INT(ADC_GAIN_8) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_16), MP_ROM_INT(ADC_GAIN_16) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_32), MP_ROM_INT(ADC_GAIN_32) },
	{ MP_ROM_QSTR(MP_QSTR_GAIN_64), MP_ROM_INT(ADC_GAIN_64) },
};

STATIC MP_DEFINE_CONST_DICT(machine_adc_locals_dict, machine_adc_locals_dict_table);

const mp_obj_type_t machine_adc_type = {
	{ &mp_type_type },
	.name = MP_QSTR_ADC,
	.print = machine_adc_print,
	.make_new = machine_adc_make_new,
	.locals_dict = (mp_obj_t)&machine_adc_locals_dict,
};
