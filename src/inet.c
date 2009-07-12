/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2009  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/ethernet.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>

#include "connman.h"

int connman_inet_ifindex(const char *name)
{
	struct ifreq ifr;
	int sk, err;

	if (name == NULL)
		return -1;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	err = ioctl(sk, SIOCGIFINDEX, &ifr);

	close(sk);

	if (err < 0)
		return -1;

	return ifr.ifr_ifindex;
}

char *connman_inet_ifname(int index)
{
	struct ifreq ifr;
	int sk, err;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	return strdup(ifr.ifr_name);
}

int connman_inet_ifup(int index)
{
	struct ifreq ifr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -errno;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (ioctl(sk, SIOCGIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (ifr.ifr_flags & IFF_UP) {
		err = -EALREADY;
		goto done;
	}

	ifr.ifr_flags |= IFF_UP;

	if (ioctl(sk, SIOCSIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	err = 0;

done:
	close(sk);

	return err;
}

int connman_inet_ifdown(int index)
{
	struct ifreq ifr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -errno;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (ioctl(sk, SIOCGIFFLAGS, &ifr) < 0) {
		err = -errno;
		goto done;
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		err = -EALREADY;
		goto done;
	}

	ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(sk, SIOCSIFFLAGS, &ifr) < 0)
		err = -errno;
	else
		err = 0;

done:
	close(sk);

	return err;
}

static unsigned short index2type(int index)
{
	struct ifreq ifr;
	int sk, err;

	if (index < 0)
		return ARPHRD_VOID;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return ARPHRD_VOID;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	if (err == 0)
		err = ioctl(sk, SIOCGIFHWADDR, &ifr);

	close(sk);

	if (err < 0)
		return ARPHRD_VOID;

	return ifr.ifr_hwaddr.sa_family;
}

static char *index2addr(int index)
{
	struct ifreq ifr;
	struct ether_addr *eth;
	char *str;
	int sk, err;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	if (err == 0)
		err = ioctl(sk, SIOCGIFHWADDR, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	str = malloc(18);
	if (!str)
		return NULL;

	eth = (void *) &ifr.ifr_hwaddr.sa_data;
	snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
						eth->ether_addr_octet[0],
						eth->ether_addr_octet[1],
						eth->ether_addr_octet[2],
						eth->ether_addr_octet[3],
						eth->ether_addr_octet[4],
						eth->ether_addr_octet[5]);

	return str;
}

static char *index2ident(int index, const char *prefix)
{
	struct ifreq ifr;
	struct ether_addr *eth;
	char *str;
	int sk, err, len;

	if (index < 0)
		return NULL;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return NULL;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	err = ioctl(sk, SIOCGIFNAME, &ifr);

	if (err == 0)
		err = ioctl(sk, SIOCGIFHWADDR, &ifr);

	close(sk);

	if (err < 0)
		return NULL;

	len = prefix ? strlen(prefix) + 18 : 18;

	str = malloc(len);
	if (!str)
		return NULL;

	eth = (void *) &ifr.ifr_hwaddr.sa_data;
	snprintf(str, len, "%s%02x%02x%02x%02x%02x%02x",
						prefix ? prefix : "",
						eth->ether_addr_octet[0],
						eth->ether_addr_octet[1],
						eth->ether_addr_octet[2],
						eth->ether_addr_octet[3],
						eth->ether_addr_octet[4],
						eth->ether_addr_octet[5]);

	return str;
}

enum connman_device_type __connman_inet_get_device_type(int index)
{
	enum connman_device_type devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
	unsigned short type = index2type(index);
	const char *devname;
	struct ifreq ifr;
	int sk;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return devtype;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0)
		goto done;

	devname = ifr.ifr_name;

	if (type == ARPHRD_ETHER) {
		char bridge_path[PATH_MAX], wimax_path[PATH_MAX];
		struct stat st;
		struct iwreq iwr;

		snprintf(bridge_path, PATH_MAX,
					"/sys/class/net/%s/bridge", devname);
		snprintf(wimax_path, PATH_MAX,
					"/sys/class/net/%s/wimax", devname);

		memset(&iwr, 0, sizeof(iwr));
		strncpy(iwr.ifr_ifrn.ifrn_name, devname, IFNAMSIZ);

		if (__connman_udev_is_mbm(devname) == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_MBM;
		else if (g_str_has_prefix(devname, "vmnet") == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (g_str_has_prefix(ifr.ifr_name, "vboxnet") == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (g_str_has_prefix(devname, "bnep") == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (g_str_has_prefix(devname, "wmx") == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (stat(wimax_path, &st) == 0 && (st.st_mode & S_IFDIR))
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (stat(bridge_path, &st) == 0 && (st.st_mode & S_IFDIR))
			devtype = CONNMAN_DEVICE_TYPE_UNKNOWN;
		else if (ioctl(sk, SIOCGIWNAME, &iwr) == 0)
			devtype = CONNMAN_DEVICE_TYPE_WIFI;
		else
			devtype = CONNMAN_DEVICE_TYPE_ETHERNET;
	} else if (type == ARPHRD_NONE) {
		if (g_str_has_prefix(devname, "hso") == TRUE)
			devtype = CONNMAN_DEVICE_TYPE_HSO;
	}

done:
	close(sk);

	return devtype;
}

struct connman_device *connman_inet_create_device(int index)
{
	enum connman_device_mode mode = CONNMAN_DEVICE_MODE_UNKNOWN;
	enum connman_device_type type;
	struct connman_device *device;
	char *addr, *name, *devname, *ident = NULL;

	if (index < 0)
		return NULL;

	devname = connman_inet_ifname(index);
	if (devname == NULL)
		return NULL;

	__connman_udev_get_devtype(devname);

	type = __connman_inet_get_device_type(index);

	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
		connman_info("Ignoring network interface %s", devname);
		g_free(devname);
		return NULL;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
	case CONNMAN_DEVICE_TYPE_WIFI:
	case CONNMAN_DEVICE_TYPE_WIMAX:
		name = index2ident(index, "");
		addr = index2addr(index);
		break;
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
	case CONNMAN_DEVICE_TYPE_GPS:
	case CONNMAN_DEVICE_TYPE_MBM:
	case CONNMAN_DEVICE_TYPE_HSO:
	case CONNMAN_DEVICE_TYPE_NOZOMI:
	case CONNMAN_DEVICE_TYPE_HUAWEI:
	case CONNMAN_DEVICE_TYPE_NOVATEL:
	case CONNMAN_DEVICE_TYPE_VENDOR:
		name = strdup(devname);
		addr = NULL;
		break;
	}

	device = connman_device_create(name, type);
	if (device == NULL) {
		g_free(devname);
		g_free(name);
		g_free(addr);
		return NULL;
	}

	switch (type) {
	case CONNMAN_DEVICE_TYPE_UNKNOWN:
	case CONNMAN_DEVICE_TYPE_VENDOR:
	case CONNMAN_DEVICE_TYPE_NOZOMI:
	case CONNMAN_DEVICE_TYPE_HUAWEI:
	case CONNMAN_DEVICE_TYPE_NOVATEL:
	case CONNMAN_DEVICE_TYPE_GPS:
		mode = CONNMAN_DEVICE_MODE_UNKNOWN;
		break;
	case CONNMAN_DEVICE_TYPE_ETHERNET:
		mode = CONNMAN_DEVICE_MODE_TRANSPORT_IP;
		ident = index2ident(index, NULL);
		break;
	case CONNMAN_DEVICE_TYPE_WIFI:
	case CONNMAN_DEVICE_TYPE_WIMAX:
		mode = CONNMAN_DEVICE_MODE_NETWORK_SINGLE;
		ident = index2ident(index, NULL);
		break;
	case CONNMAN_DEVICE_TYPE_BLUETOOTH:
		mode = CONNMAN_DEVICE_MODE_NETWORK_MULTIPLE;
		break;
	case CONNMAN_DEVICE_TYPE_MBM:
	case CONNMAN_DEVICE_TYPE_HSO:
		mode = CONNMAN_DEVICE_MODE_NETWORK_SINGLE;
		break;
	}

	connman_device_set_mode(device, mode);

	connman_device_set_index(device, index);
	connman_device_set_interface(device, devname);

	if (ident != NULL) {
		connman_device_set_ident(device, ident);
		g_free(ident);
	}

	connman_device_set_string(device, "Address", addr);

	g_free(devname);
	g_free(name);
	g_free(addr);

	return device;
}

int connman_inet_set_address(int index, struct in_addr address,
			struct in_addr netmask, struct in_addr broadcast)
{
	struct ifreq ifr;
	struct sockaddr_in *addr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		close(sk);
		return -1;
	}

	DBG("ifname %s", ifr.ifr_name);

	addr = (struct sockaddr_in *) &ifr.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_addr = address;

	err = ioctl(sk, SIOCSIFADDR, &ifr);

	if (err < 0)
		DBG("address setting failed (%s)", strerror(errno));

	addr = (struct sockaddr_in *) &ifr.ifr_netmask;
	addr->sin_family = AF_INET;
	addr->sin_addr = netmask;

	err = ioctl(sk, SIOCSIFNETMASK, &ifr);

	if (err < 0)
		DBG("netmask setting failed (%s)", strerror(errno));

	addr = (struct sockaddr_in *) &ifr.ifr_broadaddr;
	addr->sin_family = AF_INET;
	addr->sin_addr = broadcast;

	err = ioctl(sk, SIOCSIFBRDADDR, &ifr);

	if (err < 0)
		DBG("broadcast setting failed (%s)", strerror(errno));

	close(sk);

	return 0;
}

int connman_inet_clear_address(int index)
{
	struct ifreq ifr;
	struct sockaddr_in *addr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		close(sk);
		return -1;
	}

	DBG("ifname %s", ifr.ifr_name);

	addr = (struct sockaddr_in *) &ifr.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;

	//err = ioctl(sk, SIOCDIFADDR, &ifr);
	err = ioctl(sk, SIOCSIFADDR, &ifr);

	close(sk);

	if (err < 0 && errno != EADDRNOTAVAIL) {
		DBG("address removal failed (%s)", strerror(errno));
		return -1;
	}

	return 0;
}

int connman_inet_set_gateway(int index, struct in_addr gateway)
{
	struct ifreq ifr;
	struct rtentry rt;
	struct sockaddr_in *addr;
	int sk, err;

	sk = socket(PF_INET, SOCK_DGRAM, 0);
	if (sk < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = index;

	if (ioctl(sk, SIOCGIFNAME, &ifr) < 0) {
		close(sk);
		return -1;
	}

	DBG("ifname %s", ifr.ifr_name);

	memset(&rt, 0, sizeof(rt));
	rt.rt_flags = RTF_UP | RTF_HOST;

	addr = (struct sockaddr_in *) &rt.rt_dst;
	addr->sin_family = AF_INET;
	addr->sin_addr = gateway;

	addr = (struct sockaddr_in *) &rt.rt_gateway;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;

	addr = (struct sockaddr_in *) &rt.rt_genmask;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;

	rt.rt_dev = ifr.ifr_name;

	err = ioctl(sk, SIOCADDRT, &rt);
	if (err < 0)
		connman_error("Setting host gateway route failed (%s)",
							strerror(errno));

	memset(&rt, 0, sizeof(rt));
	rt.rt_flags = RTF_UP | RTF_GATEWAY;

	addr = (struct sockaddr_in *) &rt.rt_dst;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;

	addr = (struct sockaddr_in *) &rt.rt_gateway;
	addr->sin_family = AF_INET;
	addr->sin_addr = gateway;

	addr = (struct sockaddr_in *) &rt.rt_genmask;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;

	err = ioctl(sk, SIOCADDRT, &rt);
	if (err < 0)
		connman_error("Setting default route failed (%s)",
							strerror(errno));

	close(sk);

	return err;
}
