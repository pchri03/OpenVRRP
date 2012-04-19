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

#include <syslog.h>

#include <netlink/addr.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>

nl_sock *Netlink::m_socket = 0;

nl_sock *Netlink::netlinkSocket ()
{
	if (m_socket == 0)
	{
		m_socket = nl_socket_alloc();
		nl_connect(m_socket, NETLINK_ROUTE);
	}

	return m_socket;
}

bool Netlink::addIpAddress (int interface, const Addr &addr, int family)
{
	nl_addr *nlAddr = nl_addr_build(family, const_cast<void *>(reinterpret_cast<const void *>(&addr)), family == AF_INET ? 4 : 16);
	rtnl_addr *rtnlAddr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(rtnlAddr, interface);
	rtnl_addr_set_local(rtnlAddr, nlAddr);

	int ret = rtnl_addr_add(netlinkSocket(), rtnlAddr, 0);

	rtnl_addr_put(rtnlAddr);
	nl_addr_put(nlAddr);

	return (ret == 0);
}

bool Netlink::removeIpAddress (int interface, const Addr &addr, int family)
{
	nl_addr *nlAddr = nl_addr_build(family, const_cast<void *>(reinterpret_cast<const void *>(&addr)), family == AF_INET ? 4 : 16);
	rtnl_addr *rtnlAddr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(rtnlAddr, interface);
	rtnl_addr_set_local(rtnlAddr, nlAddr);
	
	int ret = rtnl_addr_delete(netlinkSocket(), rtnlAddr, 0);

	rtnl_addr_put(rtnlAddr);
	nl_addr_put(nlAddr);

	return (ret == 0);
}

int Netlink::addMacvlanInterface (int interface, const std::uint8_t *macAddress)
{
	nl_addr *addr = nl_addr_build(AF_LLC, const_cast<void *>(reinterpret_cast<const void *>(macAddress)), 6);
	rtnl_link *link = rtnl_link_alloc();

	rtnl_link_set_addr(link, addr);
	rtnl_link_set_link(link, interface);
	rtnl_link_set_type(link, "macvlan");

	int ret = rtnl_link_add(netlinkSocket(), link, 0);
	if (ret == 0)
		ret = rtnl_link_get_ifindex(link);
	else
	{
		syslog(LOG_ERR, "Error creating MACVLAN interface: %s", nl_geterror(ret));
		ret = 0;
	}
	
	rtnl_link_put(link);
	nl_addr_put(addr);

	return ret;
}

bool Netlink::removeInterface (int interface)
{
	rtnl_link *link = rtnl_link_alloc();
	rtnl_link_set_ifindex(link, interface);

	int ret = rtnl_link_delete(netlinkSocket(), link);

	rtnl_link_put(link);

	return (ret == 0);
}

bool Netlink::toggleInterface (int interface, bool up)
{
	nl_cache *cache;
	rtnl_link_alloc_cache(netlinkSocket(), AF_UNSPEC, &cache);
	rtnl_link *oldLink = rtnl_link_get(cache, interface);
	rtnl_link *link = rtnl_link_alloc();
	if (up)
		rtnl_link_set_flags(link, IFF_UP);
	else
		rtnl_link_unset_flags(link, IFF_UP);

	int ret = rtnl_link_change(netlinkSocket(), oldLink, link, 0);

	rtnl_link_put(link);
	rtnl_link_put(oldLink);
	nl_cache_free(cache);

	return (ret == 0);
}

bool Netlink::setMac (int interface, const std::uint8_t *macAddress)
{
	nl_addr *addr = nl_addr_build(AF_LLC, const_cast<void *>(reinterpret_cast<const void *>(macAddress)), 6);
	
	nl_cache *cache;
	rtnl_link_alloc_cache(netlinkSocket(), AF_UNSPEC, &cache);
	rtnl_link *oldLink = rtnl_link_get(cache, interface);
	rtnl_link *link = rtnl_link_alloc();
	rtnl_link_set_addr(link, addr);
	int ret = rtnl_link_change(netlinkSocket(), oldLink, link, 0);

	rtnl_link_put(link);
	rtnl_link_put(oldLink);
	nl_cache_free(cache);
	nl_addr_put(addr);

	return (ret == 0);

}
