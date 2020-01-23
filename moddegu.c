#include <errno.h>
#include <zephyr.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include <stdio.h>
#include <string.h>
#include <net/coap.h>

#include <power.h>
#include <net/net_if.h>
#include <net/openthread.h>
#include <openthread/thread.h>

#include "zcoap.h"
#include "degu_utils.h"
#include "degu_ota.h"
#include "degu_pm.h"

STATIC mp_obj_t degu_check_update(void) {
	return mp_obj_new_int(check_update());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(degu_check_update_obj, degu_check_update);

STATIC mp_obj_t degu_update_shadow(mp_obj_t shadow) {
	int ret = degu_coap_request("thing", COAP_METHOD_POST, (u8_t *)mp_obj_str_get_str(shadow), NULL);

	return mp_obj_new_int(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(degu_update_shadow_obj, degu_update_shadow);

STATIC mp_obj_t degu_get_shadow(void) {
	vstr_t vstr;
	int ret;
	u8_t *payload = (u8_t *)m_malloc(MAX_COAP_MSG_LEN);

	if (!payload) {
		printf("can't malloc\n");
		return mp_const_none;
	}
	memset(payload, 0, MAX_COAP_MSG_LEN);

	ret = degu_coap_request("thing", COAP_METHOD_GET, payload, NULL);

	if (payload != NULL && ret >= COAP_RESPONSE_CODE_OK) {
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

STATIC mp_obj_t mod_suspend(mp_obj_t time_sec)
{
	s32_t time_to_wake = mp_obj_get_int(time_sec);

	struct net_if *iface;
	struct openthread_context *ot_context;
	otLinkModeConfig config;
	uint8_t channel;

	iface = net_if_get_default();
	ot_context = net_if_l2_data(iface);
	channel = otLinkGetChannel(ot_context->instance);
	config = otThreadGetLinkMode(ot_context->instance);

	sys_pm_ctrl_enable_state(SYS_POWER_STATE_SLEEP_3);
	sys_set_power_state(SYS_POWER_STATE_SLEEP_3);

	openthread_suspend(ot_context->instance);
	k_sleep(K_SECONDS(time_to_wake));
	openthread_resume(ot_context->instance, channel, config);

	sys_pm_ctrl_disable_state(SYS_POWER_STATE_SLEEP_3);

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_suspend_obj, mod_suspend);

STATIC mp_obj_t mod_powerdown(void)
{
	degu_ext_device_power(false);
	sys_pm_suspend_devices();

	sys_pm_ctrl_enable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
	sys_set_power_state(SYS_POWER_STATE_DEEP_SLEEP_1);
	sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);

	sys_pm_resume_devices();
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_powerdown_obj, mod_powerdown);

STATIC const mp_rom_map_elem_t degu_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_degu) },
	{ MP_ROM_QSTR(MP_QSTR_check_update), MP_ROM_PTR(&degu_check_update_obj) },
	{ MP_ROM_QSTR(MP_QSTR_update_shadow), MP_ROM_PTR(&degu_update_shadow_obj) },
	{ MP_ROM_QSTR(MP_QSTR_get_shadow), MP_ROM_PTR(&degu_get_shadow_obj) },
	{ MP_ROM_QSTR(MP_QSTR_suspend), MP_ROM_PTR(&mod_suspend_obj) },
	{ MP_ROM_QSTR(MP_QSTR_powerdown), MP_ROM_PTR(&mod_powerdown_obj) },
};

STATIC MP_DEFINE_CONST_DICT (mp_module_degu_globals, degu_globals_table);

const mp_obj_module_t mp_module_degu = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t*)&mp_module_degu_globals,
};
