/*
 * Copyright (C) 2012 Peter Christensen <pch@ordbogen.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "netlink.h"
#include "mainloop.h"

#include <cstring>
#include <cstdio>
#include <fstream>

#include <dirent.h>
#include <syslog.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/if_link.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>

nl_sock *Netlink::createSocket ()
{
	nl_sock *sock = nl_socket_alloc();
	int err = nl_connect(sock, NETLINK_ROUTE);
	if (err != 0)
	{
		syslog(LOG_ERR, "Error creating netlink socket: %s", nl_geterror(err));
		nl_socket_free(sock);
		return 0;
	}

	return sock;
}

IpAddress Netlink::getPrimaryIpAddress (int interface, int family)
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return IpAddress();

	nl_cache *cache;
	rtnl_addr_alloc_cache(sock, &cache);

	IpAddress address;
	for (nl_object *it = nl_cache_get_first(cache); it != 0; it = nl_cache_get_next(it))
	{
		rtnl_addr *addr = reinterpret_cast<rtnl_addr *>(it);
		if (
				rtnl_addr_get_ifindex(addr) == interface &&
				rtnl_addr_get_flags(addr) & IFA_F_PERMANENT)
		{
			nl_addr *local = rtnl_addr_get_local(addr);
			address = IpAddress(nl_addr_get_binary_addr(local), rtnl_addr_get_family(addr));
			break;
		}
	}

	nl_cache_free(cache);

	if (address.family() == AF_UNSPEC)
		syslog(LOG_WARNING, "Unable to get a local address for interface %i", interface);

	return address;
}

bool Netlink::addIpAddress (int interface, const IpSubnet &ip)
{
	return modifyIpAddress(interface, ip, true);
}

bool Netlink::removeIpAddress (int interface, const IpSubnet &ip)
{
	return modifyIpAddress(interface, ip, false);
}

bool Netlink::modifyIpAddress (int interface, const IpSubnet &ip, bool add)
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return false;

	nl_addr *local = nl_addr_build(ip.address().family(), const_cast<void *>(ip.address().data()), ip.address().size());

	rtnl_addr *addr = rtnl_addr_alloc();
	rtnl_addr_set_scope(addr, RT_SCOPE_UNIVERSE);
	rtnl_addr_set_ifindex(addr, interface);
	rtnl_addr_set_family(addr, ip.address().family());
	rtnl_addr_set_local(addr, local);
	rtnl_addr_set_prefixlen(addr, ip.cidr());

	int err;
	if (add)
		err = rtnl_addr_add(sock, addr, NLM_F_CREATE | NLM_F_EXCL);
	else
		err = rtnl_addr_delete(sock, addr, 0);

	rtnl_addr_put(addr);
	nl_addr_put(local);

	nl_socket_free(sock);

	if (err != 0)
	{
		if (add)
			syslog(LOG_ERR, "Error adding IP address %s to interface %i: %s", ip.toString().c_str(), interface, nl_geterror(err));
		else
			syslog(LOG_WARNING, "Error removing IP address %s from interface %i: %s", ip.toString().c_str(), interface, nl_geterror(err));
		return false;
	}
	else
		return true;
}


int Netlink::addMacvlanInterface (int interface, const std::uint8_t *macAddress, const char *name)
{
	// RTM_NEWLINK:
	// IFLA_IFNAME = name
	// IFLA_ADDRESS = macAddress
	// IFLA_LINK = interface
	// IFLA_LINKINFO = {
	//   IFLA_INFO_KIND = macvlan
	//   IFLA_INFO_DATA {
	//     IFLA_MACVLAN_MODE = MACVLAN_MODE_VEPA
	//   }
	// }

	nl_msg *msg = nlmsg_alloc_simple(RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL);

	ifinfomsg infomsg;
	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;

	nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO);
	nla_put_string(msg, IFLA_IFNAME, name);
	nla_put(msg, IFLA_ADDRESS, 6, macAddress);
	nla_put_u32(msg, IFLA_OPERSTATE, 6);
	nla_put_u32(msg, IFLA_LINK, interface);
	{
		nl_msg *linkinfo = nlmsg_alloc();
		nla_put_string(linkinfo, IFLA_INFO_KIND, "macvlan");
		{
			nl_msg *infodata = nlmsg_alloc();
			nla_put_u32(infodata, IFLA_MACVLAN_MODE, MACVLAN_MODE_VEPA);
			nla_put_nested(linkinfo, IFLA_INFO_DATA, infodata);
			nlmsg_free(infodata);
		}
		nla_put_nested(msg, IFLA_LINKINFO, linkinfo);
		nlmsg_free(linkinfo);
	}

	nl_sock *sock = createSocket();
	if (sock == 0)
	{
		nlmsg_free(msg);
		return -1;
	}

	int err = nl_send_auto_complete(sock, msg);

	nlmsg_free(msg);

	if (err >= 0)
		err = nl_wait_for_ack(sock);

	if (err < 0)
	{
		syslog(LOG_ERR, "Error creating interface: %s", nl_geterror(err));
		nl_socket_free(sock);
		return -1;
	}

	nl_cache *cache;

	rtnl_link_alloc_cache(sock, &cache);

	// Set sysctl parameters

	// Interface are set up so that both interfaces will:
	// - reply to ARP only if the target IP address is local address configured on the incoming interface (arp_ignore=1)
	// - try to avoid local addresses that are not in the target's subnet for this interface (arp_announce=1)
	// The real interface (and ideally all other interfaces) will
	// - allow to have multiple network interfaces on the same  subnet, and have the ARPs for each interface be answered based on whether or not the kernel would route a packet from the ARP'd IP out that interface (arp_filter=1)
	// But the MACVLAN interface will
	// - Respond to the ARP requests with addresses from other interfaces (arp_filter=0)

	char nameBuffer[IFNAMSIZ];
	rtnl_link_i2name(cache, interface, nameBuffer, sizeof(nameBuffer))
	setIpConfiguration(nameBuffer, "arp_ignore", "1");
	setIpConfiguration(nameBuffer, "arp_announce", "1");
	setIpConfiguration(nameBuffer, "arp_filter", "1");

	setIpConfiguration(name, "arp_ignore", "1");
	setIpConfiguration(name, "arp_announce", "1");
	setIpConfiguration(name, "arp_filter", "0");

	int newInterface = rtnl_link_name2i(cache, name);

	nl_cache_free(cache);
	nl_socket_free(sock);

	return newInterface;
}

bool Netlink::removeInterface (int interface)
{
	// RTM_DELLINK:
	nl_sock *sock = createSocket();

	nl_msg *msg = nlmsg_alloc_simple(RTM_DELLINK, NLM_F_REQUEST);
	ifinfomsg infomsg;

	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;
	infomsg.ifi_index = interface;

	nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO);

	int err = nl_send_auto_complete(sock, msg);
	nlmsg_free(msg);

	if (err >= 0)
		err = nl_wait_for_ack(sock);

	nl_socket_free(sock);

	if (err < 0)
	{
		syslog(LOG_WARNING, "Error removing interface %i: %s", interface, nl_geterror(err));
		return false;
	}
	else
		return true;
}

InterfaceList Netlink::interfaces ()
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return InterfaceList();

	nl_cache *cache;
	rtnl_link_alloc_cache(sock, &cache);

	InterfaceList list;

	for (nl_object *it = nl_cache_get_first(cache); it != 0; it = nl_cache_get_next(it))
	{
		rtnl_link *link = reinterpret_cast<rtnl_link *>(it);
		list[rtnl_link_get_ifindex(link)] = rtnl_link_get_name(link);
	}

	nl_cache_free(cache);
	nl_socket_free(sock);

	return list;
}

bool Netlink::setMac (int interface, const std::uint8_t *macAddress)
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return false;

	nl_msg * msg = nlmsg_alloc_simple(RTM_SETLINK, 0);

	ifinfomsg infomsg;
	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;
	infomsg.ifi_index = interface;

	nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO);
	nla_put(msg, IFLA_ADDRESS, 6, macAddress);

	int err = nl_send_auto_complete(sock, msg);
	if (err >= 0)
		err = nl_wait_for_ack(sock);

	nlmsg_free(msg);

	if (err < 0)
		syslog(LOG_ERR, "Error setting MAC address of interface %i: %s", nl_geterror(err));

	nl_socket_free(sock);

	return (err >= 0);
}

bool Netlink::isInterfaceUp (int interface)
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return false;

	nl_cache *cache;
	rtnl_link_alloc_cache(sock, &cache);

	bool up = false;

	rtnl_link *link = rtnl_link_get(cache, interface);
	if (link != 0)
	{
		if (rtnl_link_get_operstate(link) == 6) // IF_OPER_UP
			up = true;
	}

	nl_cache_free(cache);
	nl_socket_free(sock);

	return up;
}

bool Netlink::toggleInterface (int interface, bool up)
{
	nl_sock *sock = createSocket();
	if (sock == 0)
		return false;

	nl_msg * msg = nlmsg_alloc_simple(RTM_SETLINK, 0);

	ifinfomsg infomsg;
	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;
	infomsg.ifi_index = interface;
	infomsg.ifi_change = IFF_UP;
	infomsg.ifi_flags = (up ? IFF_UP : 0);

	nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO);

	int err = nl_send_auto_complete(sock, msg);
	if (err >= 0)
		err = nl_wait_for_ack(sock);

	nlmsg_free(msg);

	if (err < 0)
		syslog(LOG_ERR, "Error toggling interface %i: %s", nl_geterror(err));

	nl_socket_free(sock);

	return (err >= 0);
}

bool Netlink::setIpConfiguration (const char *interface, const char *parameter, const char *value)
{
	std::string path("/proc/sys/net/ipv4/conf/");
	path.append(interface).append("/").append(parameter);

	std::ofstream file(path.c_str());
	if (file.good())
	{
		file << value;
		file.close();
		return true;
	}
	else
		return false;
}

Netlink::CallbackMap Netlink::callbacks;
nl_sock *Netlink::sock = 0;

bool Netlink::addInterfaceMonitor (int interface, InterfaceCallback *callback, void *userData)
{
	CallbackData data(callback, userData);

	// Check if the callback already exists
	CallbackMap::iterator it = callbacks.find(interface);
	if (it == callbacks.end())
	{
		std::pair<CallbackMap::iterator,bool> ret = callbacks.insert(CallbackMap::value_type(interface, CallbackDataSet()));
		if (!ret.second)
			return false;
		else
			it = ret.first;
	}
	else
	{
		if (it->second.find(data) != it->second.end())
			return false;
		else
		{
			it->second.insert(data);
			return true;
		}
	}

	if (sock == 0)
	{
		sock = nl_socket_alloc();

		int ret = nl_connect(sock, NETLINK_ROUTE);
		if (ret != 0)
		{
			syslog(LOG_ERR, "Error creating NETLINK_ROUTE netlink socket: %i", ret);

			callbacks.erase(it);
			return false;
		}

		nl_socket_set_nonblocking(sock);
		nl_socket_modify_cb(sock, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, nlSequenceCallback, 0);
		nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, nlMessageCallback, sock);
		nl_socket_add_membership(sock, RTMGRP_LINK);

		MainLoop::addMonitor(nl_socket_get_fd(sock), nlSocketCallback, sock);
	}

	it->second.insert(data);
	return true;
}

bool Netlink::removeInterfaceMonitor (int interface, InterfaceCallback *callback, void *userData)
{
	CallbackData data(callback, userData);

	// Check if the callback already exists
	CallbackMap::iterator it = callbacks.find(interface);
	if (it == callbacks.end())
		return false;

	CallbackDataSet::iterator dataIt = it->second.find(data);
	if (dataIt == it->second.end())
		return false;

	it->second.erase(dataIt);
	if (it->second.size() == 0)
	{
		callbacks.erase(it);
		if (callbacks.size() == 0)
		{
			MainLoop::removeMonitor(nl_socket_get_fd(sock));
			nl_socket_free(sock);
			sock = 0;
		}
	}

	return true;
}

void Netlink::nlSocketCallback (int, void *userData)
{
	nl_sock *sock = reinterpret_cast<nl_sock *>(userData);
	int err;
	while ((err = nl_recvmsgs_default(sock)) > 0);
	if (err < 0)
		syslog(LOG_WARNING, "Error receiving netlink message: %s", nl_geterror(err));
}

int Netlink::nlMessageCallback (nl_msg *msg, void *)
{
	nlmsghdr *hdr = nlmsg_hdr(msg);

	if (hdr->nlmsg_type == RTM_NEWLINK)
	{
		const ifinfomsg *msg = reinterpret_cast<const ifinfomsg *>(nlmsg_data(hdr));

		CallbackMap::const_iterator interfaceIt = callbacks.find(msg->ifi_index);
		if (interfaceIt != callbacks.end())
		{
			bool isUp = (msg->ifi_flags & IFF_UP) == IFF_UP;
			for (CallbackDataSet::const_iterator it = interfaceIt->second.begin(); it != interfaceIt->second.end(); ++it)
			{
				it->first(msg->ifi_index, isUp, it->second);
			}
		}
	}
	return NL_OK;
}

int Netlink::nlSequenceCallback (nl_msg *, void *)
{
	return NL_OK;
}
