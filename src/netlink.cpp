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

bool Netlink::modifyIpAddress (int interface, const Ip &ip, bool add)
{
	Attribute attr(IFA_LOCAL, ip->data(), ip->size());

	std::vector<std::uint8_t> buffer;
	buffer.reserve(16 + 16 + attr.size());

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer.data());
	hdr->nlmsg_len = buffer.size();
	hdr->nlmsg_type = (add ? RTM_NEWADDR : RTM_DELADDR);
	hdr->nlmsg_flags = NLM_F_REQUEST | (add ? NLM_F_CREATE | NLM_F_EXCL : 0);
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifaddrmsg *msg = reinterpret_cast<ifaddrmsg *>(buffer.data() + 16);
	msg->ifa_family = ip.family();
	msg->ifa_prefixlen = ip.size() * 8;
	msg->ifa_flags = 0;
	msg->ifa_scope = RT_SCOPE_LINK;
	msg->ifa_index = interface;

	attr.toPacket(buffer.data() + 32);

	return sendNetlinkPacket(buffer.data(), buffer.size()) == 0;
}

Netlink::Attribute::Attribute (std::uint16_t type, const void *data, unsigned int size) :
	m_type(type)
{
	if (size > 0)
	{
		m_buffer.reserve(size);
		std::memcpy(m_buffer.data(), data, size);
	}
}

Netlink::Attribute::~Attribute ()
{
}

Netlink::Attribute::addAttribute (const Attribute &attribute)
{
	m_attributes.push_back(attribute);
}

unsigned int Netlink::Attribute::size () const
{
	unsigned int size = 4;
	if (m_attributes.size() > 0)
	{
		for (AttributeList::const_iterator attribute = m_attributes.begin(); attribute != m_attributes.end(); ++attribute)
		{
			size += attribute->size();
		}
	}
	else
	{
		size += m_buffer.size();
		if (size & 0x03)
			size = (size & ~0x03) + 4;
	}

	return size;
}

void Netlink::Attribute::toPacket (void *buffer) const
{
	std::uint8_t *ptr = reinterpret_cast<std::uint8_t *>(buffer);
	*reinterpret_cast<std::uint16_t *>(ptr) = size();
	*reinterpret_cast<std::uint16_t *>(ptr + 2) = m_type;
	if (m_attribute.size() > 0)
	{
		ptr += 4;
		for (AttributeList::const_iterator attribute = m_attributes.begin(); attribute != m_attributes.end(); ++attribute)
		{
			attribute->comple(ptr);
			ptr += attribute->size();
		}
	}
	else if (m_buffer.size() > 0)
	{
		std::memcpy(ptr + 4, m_buffer.data(), m_buffer.size());
		if (m_buffer.size() & 0x03 != 0)
			std::memset(ptr + 4 + m_buffer.size(), 0, 4 - (m_buffer.size() & 0x03));
	}
}

/*
#include <netlink/addr.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>
#include <netlink/version.h>

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
#ifdef LIBNL_VER_NUM
	rtnl_link_set_type(link, "macvlan");
#else
	rtnl_link_set_info_type(link, "macvlan");
#endif

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
*/
