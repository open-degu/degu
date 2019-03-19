#include <errno.h>
#include <zephyr.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"

//#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }
#define RAISE_SYSCALL_ERRNO(x) { if ((int)(x) == -1) mp_raise_OSError(errno); }

typedef struct _mp_obj_coap_t {
	mp_obj_base_t base;
	//struct coap_dtls
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
	//TODO:check args: mp_arg_check_num(n_args, n_kw, 1, 1, false);
	printf("new coap obj\n");
	mp_obj_coap_t *client = m_new_obj_with_finaliser(mp_obj_coap_t);
	client->base.type = (mp_obj_t)&coap_type;
	//client->base.type = type;
	client->sock = zsock_socket(AF_INET6, SOCK_DGRAM, 0);
	//printf("Hello INIT%s!\n", mp_obj_str_get_str(args));
	//TODO: check type of args[0] is tuple like ('ff03::1', 1234)
	//parse_inet_addr(client, args[0], &sockaddr);
	//zsock_connect(client->sock, &sockaddr, sizeof(sockaddr));

	return MP_OBJ_FROM_PTR(client);
}

STATIC mp_obj_t coap_dump(void) {
	printf("dump\n");
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(coap_dump_obj, coap_dump);

#define MAX_COAP_MSG_LEN 256
#define COAP_TYPE_CON 0 //Confirmable
#define COAP_TYPE_NCON 1 //non-Confirmable
#define COAP_TYPE_ACK 2 //Acknowledgement
#define COAP_TYPE_RST 3 //Reset

static const char * const test_path[] = { "gateway", NULL };

STATIC mp_obj_t coap_request(mp_obj_t self_in) {
	printf("requested!");
	mp_obj_coap_t *client = self_in;
	int r;
	struct coap_packet request;
	u8_t method = COAP_METHOD_POST;
	u8_t *data;
	u8_t payload[] = "{\"payload\": \"TEST_PAY\" }";
	const char * const *p;

	data = (u8_t *)malloc(MAX_COAP_MSG_LEN);
	if (!data) {
		printf("can't malloc\n");
		RAISE_SYSCALL_ERRNO(-1);
	}

	r = coap_packet_init(&request, data, MAX_COAP_MSG_LEN,
			     1, COAP_TYPE_CON, 8, coap_next_token(),
			     method, coap_next_id());
	for (p = test_path; p && *p; p++) {
		r = coap_packet_append_option(&request, COAP_OPTION_URI_PATH,
					      *p, strlen(*p));
		if (r < 0) {
			printf("Unable add option to request");
			goto end;
		}
	}

	r = coap_packet_append_payload_marker(&request);
	if (r < 0) {
		printf("Unable append payload maker");
		goto end;
	}
	r = coap_packet_append_payload(&request, (u8_t *)payload,
				       sizeof(payload) - 1);
	if (r < 0) {
		printf("Not able to append payload");
		goto end;
	}
	//TODO: debug dump

	r = zsock_send(client->sock, request.data, request.offset, 0);

end:
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(coap_request_obj, coap_request);

//it may be obsolescent
STATIC mp_obj_t coap_connect(mp_obj_t self_in, mp_obj_t address) {
	mp_obj_coap_t *client = self_in;
	struct sockaddr sockaddr;
	printf("parse\n");
	parse_inet_addr(client, address, &sockaddr);
	printf("connect\n");
	zsock_connect(client->sock, &sockaddr, sizeof(sockaddr));
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(coap_connect_obj, coap_connect);

STATIC mp_obj_t coap_close(mp_obj_t self_in) {
	mp_obj_coap_t *self = self_in;
	zsock_close(self->sock);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(coap_close_obj, coap_close);


STATIC const mp_rom_map_elem_t coap_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&coap_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&coap_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR_dump), MP_ROM_PTR(&coap_dump_obj) },
	{ MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&coap_connect_obj) },
	{ MP_ROM_QSTR(MP_QSTR_request), MP_ROM_PTR(&coap_request_obj) },
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
};

STATIC MP_DEFINE_CONST_DICT (mp_module_zcoap_globals, zcoap_globals_table);

const mp_obj_module_t mp_module_zcoap = {
	.base = { &mp_type_module },
	.globals = (mp_obj_dict_t*)&mp_module_zcoap_globals,
};
