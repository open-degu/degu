#include <errno.h>
#include <zephyr.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

#include <stdio.h>
#include <string.h>

#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>

#include "zcoap.h"
#include "degu_utils.h"

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }
#define RAISE_SYSCALL_ERRNO(x) { if ((int)(x) == -1) mp_raise_OSError(errno); }

typedef struct _mp_obj_coap_t {
	mp_obj_base_t base;
	int sock;
} mp_obj_coap_t;

STATIC const mp_obj_type_t coap_type;

STATIC void parse_inet_addr(mp_obj_coap_t *coap, mp_obj_t addr_in, struct sockaddr *sockaddr) {
    // We employ the fact that port and address offsets are the same for IPv4 & IPv6
    struct sockaddr_in *sockaddr_in = (struct sockaddr_in*)sockaddr;

    mp_obj_t *addr_items;
    mp_obj_get_array_fixed_n(addr_in, 2, &addr_items);
    sockaddr_in->sin_family = net_context_get_family((void*)coap->sock);
    RAISE_ERRNO(net_addr_pton(sockaddr_in->sin_family, mp_obj_str_get_str(addr_items[0]), &sockaddr_in->sin_addr));
    sockaddr_in->sin_port = htons(mp_obj_get_int(addr_items[1]));
}

STATIC mp_obj_t coap_client_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
	struct sockaddr sockaddr;
	int ret;

	//TODO:check args: mp_arg_check_num(n_args, n_kw, 1, 1, false);
	mp_obj_coap_t *client = m_new_obj_with_finaliser(mp_obj_coap_t);
	client->base.type = (mp_obj_t)&coap_type;

	client->sock = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	RAISE_SYSCALL_ERRNO(client->sock);

	parse_inet_addr(client, args[0], &sockaddr);
	ret = zsock_connect(client->sock, &sockaddr, sizeof(sockaddr));
	RAISE_SYSCALL_ERRNO(ret);

	return MP_OBJ_FROM_PTR(client);
}

STATIC mp_obj_t coap_request_post(mp_obj_t self_in, mp_obj_t path, mp_obj_t payload) {
	mp_obj_coap_t *client = self_in;
	u8_t *str_code = zcoap_request_post(client->sock,
					(u8_t *)mp_obj_str_get_str(path),
					(u8_t *)mp_obj_str_get_str(payload));

	if (str_code != NULL)
		return mp_obj_new_str(str_code, strlen(str_code));
	else
		return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(coap_request_post_obj, coap_request_post);

STATIC mp_obj_t coap_request_get(mp_obj_t self_in, mp_obj_t path) {
	mp_obj_coap_t *client = self_in;
	vstr_t vstr;
	u16_t payload_len;
	u8_t *payload = zcoap_request_get(client->sock,
					(u8_t *)mp_obj_str_get_str(path), &payload_len);

	if (payload != NULL) {
		vstr_init_len(&vstr, payload_len);
		strcpy(vstr.buf, payload);
		return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
	}
	else {
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(coap_request_get_obj, coap_request_get);

STATIC mp_obj_t coap_close(mp_obj_t self_in) {
	mp_obj_coap_t *self = self_in;
	zsock_close(self->sock);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(coap_close_obj, coap_close);

STATIC mp_obj_t coap_eui64(void) {
	char eui64[17];
	get_eui64(eui64);
	eui64[16] = '\0';
	return mp_obj_new_str(eui64, strlen(eui64));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(coap_eui64_obj, coap_eui64);

STATIC mp_obj_t coap_gw_addr(void) {
	char gw_addr[NET_IPV6_ADDR_LEN];
	strcpy(gw_addr, get_gw_addr(64));

	if (gw_addr != NULL) {
		return mp_obj_new_str(gw_addr, strlen(gw_addr));
	}
	else {
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(coap_gw_addr_obj, coap_gw_addr);

STATIC const mp_rom_map_elem_t coap_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&coap_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&coap_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR_request_post), MP_ROM_PTR(&coap_request_post_obj) },
	{ MP_ROM_QSTR(MP_QSTR_request_get), MP_ROM_PTR(&coap_request_get_obj) },
};

STATIC MP_DEFINE_CONST_DICT(coap_locals_dict, coap_locals_dict_table);

STATIC const mp_obj_type_t coap_type = {
    { &mp_type_type },
    .name = MP_QSTR_client,
    .make_new = coap_client_make_new,
    .locals_dict = (void*)&coap_locals_dict,
};

STATIC const mp_rom_map_elem_t zcoap_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_zcoap) },
	{MP_ROM_QSTR(MP_QSTR_client), MP_ROM_PTR(&coap_type) },
	{MP_ROM_QSTR(MP_QSTR_eui64), MP_ROM_PTR(&coap_eui64_obj) },
	{MP_ROM_QSTR(MP_QSTR_gw_addr), MP_ROM_PTR(&coap_gw_addr_obj) },
};

STATIC MP_DEFINE_CONST_DICT (mp_module_zcoap_globals, zcoap_globals_table);

const mp_obj_module_t mp_module_zcoap = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t*)&mp_module_zcoap_globals,
};
