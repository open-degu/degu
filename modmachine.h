#ifndef MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H
#define MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H

#include "py/obj.h"
#include "uart.h"

extern const mp_obj_type_t machine_pin_type;
extern const mp_obj_type_t machine_adc_type;
extern const mp_obj_type_t machine_i2c_type;
extern const mp_obj_type_t machine_uart_type;

MP_DECLARE_CONST_FUN_OBJ_0(machine_info_obj);

typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    struct device *port;
    uint32_t pin;
} machine_pin_obj_t;

typedef struct _machine_adc_obj_t {
	mp_obj_base_t base;
	struct device *dev;
	uint8_t adc_channel;
	struct adc_channel_cfg *cfg;
	u16_t buffer;
} machine_adc_obj_t;

typedef struct _machine_i2c_obj_t {
	mp_obj_base_t base;
	struct device *dev;
	uint8_t i2c_channel;
	struct i2c_channel_cfg *cfg;
	u16_t buffer;
} machine_i2c_obj_t;

typedef struct _machine_uart_obj_t {
	mp_obj_base_t base;
	struct device *dev;
	uint8_t uart_channel;
	struct uart_config cfg;
} machine_uart_obj_t;

#endif // MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H
