/*
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
#include <net/net_ip.h>
#include <net/net_if.h>
extern char *net_byte_to_hex(char *ptr, u8_t byte, char base, bool pad);
extern char *net_sprint_addr(sa_family_t af, const void *addr);

void get_eui64(char *eui64)
{
	struct net_if *iface;
	char *buf = eui64;
	char byte;
	int i;

	iface = net_if_get_by_index(1);

	for (i = 0; i < 8; i++) {
		byte = (char)net_if_get_link_addr(iface)->addr[i];
		buf = net_byte_to_hex(buf, byte, 'A', true);
		(*buf)++;
	}
}

char *get_gw_addr(unsigned int prefix)
{
	struct net_if *iface;
	struct net_if_ipv6 *ipv6;
	struct net_if_addr *unicast;
	struct in6_addr gw_in6_addr;
	int i, j;

	iface = net_if_get_by_index(1);
	ipv6 = iface->config.ip.ipv6;

	for (i = 0; ipv6 && i < NET_IF_MAX_IPV6_ADDR; i++) {
		unicast = &ipv6->unicast[i];

		if (!unicast->is_used) {
			continue;
		}

		if (unicast->address.in6_addr.s6_addr[0] == 0xfd) {
			for (j = 0; j < 16; j++) {
				gw_in6_addr.s6_addr[j] = unicast->address.in6_addr.s6_addr[j];
				if (j >= prefix / 8) {
					gw_in6_addr.s6_addr[j] = 0x00;
				}
			}
			gw_in6_addr.s6_addr[15] += 1;
			return net_sprint_addr(AF_INET6, &gw_in6_addr);
		}
	}

	return NULL;
}
