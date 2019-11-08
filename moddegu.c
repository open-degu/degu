#include <errno.h>
#include <zephyr.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include <stdio.h>
#include <string.h>
#include <net/coap.h>

#include "zcoap.h"
#include "degu_utils.h"

STATIC mp_obj_t degu_update_shadow(mp_obj_t shadow) {
	int ret = degu_coap_request("thing", COAP_METHOD_POST, (u8_t *)mp_obj_str_get_str(shadow), NULL);

	return mp_obj_new_int(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(degu_update_shadow_obj, degu_update_shadow);

STATIC mp_obj_t degu_get_shadow(void) {
	vstr_t vstr;
	u8_t *payload = (u8_t *)m_malloc(MAX_COAP_MSG_LEN);

	if (!payload) {
		printf("can't malloc\n");
		return mp_const_none;
	}

	degu_coap_request("thing", COAP_METHOD_GET, payload, NULL);

	if (payload != NULL) {
		vstr_init_len(&vstr, strlen(payload));
		strcpy(vstr.buf, payload);
		m_free(payload);
		return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
	}
	else {
		m_free(payload);
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(degu_get_shadow_obj, degu_get_shadow);

STATIC const mp_rom_map_elem_t degu_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_degu) },
	{ MP_ROM_QSTR(MP_QSTR_update_shadow), MP_ROM_PTR(&degu_update_shadow_obj) },
	{ MP_ROM_QSTR(MP_QSTR_get_shadow), MP_ROM_PTR(&degu_get_shadow_obj) },
};

STATIC MP_DEFINE_CONST_DICT (mp_module_degu_globals, degu_globals_table);

const mp_obj_module_t mp_module_degu = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t*)&mp_module_degu_globals,
};
