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

#include <cstring>

#include <cstdio>

#include <syslog.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>

int Netlink::sendNetlinkPacket (const void *data, unsigned int size, int family, IpAddress *address, int *interface)
{
	int s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (s == -1)
	{
		std::perror("socket");
		return -1;
	}

	sockaddr_nl addr;
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
	if (bind(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		std::perror("bind");
		close(s);
		return -1;
	}

	if (write(s, data, size) == -1)
	{
		std::perror("write");
		close(s);
		return -1;
	}

	uint8_t buffer[4096];
	int ret = 0;
	const nlmsghdr *hdr = reinterpret_cast<const nlmsghdr *>(buffer);
	do
	{
		unsigned int size = read(s, buffer, sizeof(buffer));
		if (s == (unsigned int)-1)
		{
			std::perror("write");
			return -1;
		}

		hdr = reinterpret_cast<const nlmsghdr *>(buffer);

		const std::uint8_t *ptr = buffer;
		while (size >= NLA_ALIGN(hdr->nlmsg_len) && hdr->nlmsg_len >= 16)
		{
			if (hdr->nlmsg_type == NLMSG_ERROR)
			{
				if (hdr->nlmsg_len >= 32)
				{
					const nlmsgerr *err = reinterpret_cast<const nlmsgerr *>(ptr + 16);
					ret = err->error;
				}
			}
			else if (hdr->nlmsg_type == RTM_NEWLINK || hdr->nlmsg_type == RTM_GETLINK || hdr->nlmsg_type == RTM_SETLINK)
			{
				if (interface != 0 && hdr->nlmsg_len >= 32)
				{
					const ifinfomsg *msg = reinterpret_cast<const ifinfomsg *>(ptr + 16);
					*interface = msg->ifi_index;
				}
			}
			else if (hdr->nlmsg_type == RTM_NEWADDR || hdr->nlmsg_type == RTM_GETADDR || hdr->nlmsg_type == RTM_DELADDR)
			{
				if (interface != 0 && address != 0 && hdr->nlmsg_len >= 24)
				{
					const ifaddrmsg *msg = reinterpret_cast<const ifaddrmsg *>(ptr + 16);
					if (*interface == msg->ifa_index)
					{
						unsigned int left = hdr->nlmsg_len - 24;
						const std::uint8_t *attrptr = ptr + 24;
						while (left >= 4)
						{
							const nlattr *attr = reinterpret_cast<const nlattr *>(attrptr);
							if (address != 0 && (attr->nla_type == IFA_LOCAL || attr->nla_type == IFA_ADDRESS))
							{
								IpAddress addr(attrptr + 4, family);
								if (family == AF_INET6)
								{
									if (!IN6_IS_ADDR_LINKLOCAL(addr.data()))
									{
										*address = addr;
										break;
									}
								}
								else // if (family == AF_INET)
								{
									*address = IpAddress(attrptr + 4, family);
									break;
								}
							}
	
							left -= NLA_ALIGN(attr->nla_len);
							attrptr += NLA_ALIGN(attr->nla_len);
						}
					}
				}
			}

			ptr += NLA_ALIGN(hdr->nlmsg_len);
			size -= NLA_ALIGN(hdr->nlmsg_len);
		}
	} while (hdr->nlmsg_flags & NLM_F_MULTI && hdr->nlmsg_type != NLMSG_DONE);

	close(s);

	return ret;
}

IpAddress Netlink::getPrimaryIpAddress (int interface, int family)
{
	std::uint8_t buffer[sizeof(nlmsghdr) + sizeof(ifaddrmsg)];

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer);
	hdr->nlmsg_len = sizeof(buffer);
	hdr->nlmsg_type = RTM_GETADDR;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_MATCH;
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifaddrmsg *msg = reinterpret_cast<ifaddrmsg *>(buffer + 16);
	msg->ifa_family = family;
	msg->ifa_prefixlen = 0;
	msg->ifa_flags = 0;
	msg->ifa_scope = 0;
	msg->ifa_index = interface;

	IpAddress address;
	int err;
	err = sendNetlinkPacket(buffer, sizeof(buffer), family, &address, &interface);
	if (err >= 0)
		return address;
	else
	{
		syslog(LOG_WARNING, "Netlink: Error getting primary address: %s", std::strerror(0 - err));
		return IpAddress();
	}
}

bool Netlink::modifyIpAddress (int interface, const IpAddress &ip, bool add)
{
	Attribute attr(IFA_LOCAL, ip.data(), ip.size());

	std::vector<std::uint8_t> buffer;
	buffer.resize(16 + 8 + attr.effectiveSize());

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

	attr.toPacket(buffer.data() + 16 + 8);

	int err = sendNetlinkPacket(buffer.data(), buffer.size());
	if (err >= 0)
		return true;
	else
	{
		syslog(LOG_WARNING, "Netlink: Error modifying IP address: %s", std::strerror(0 - err));
		return false;
	}
}

int Netlink::addMacvlanInterface (int interface, const std::uint8_t *macAddress, const char *name)
{
	// IFLA_IFNAME = name
	// IFLA_ADDRESS = macAddress
	// IFLA_LINK = interface
	// IFLA_LINKINFO = {
	//   IFLA_INFO_KIND = macvlan
	//   IFLA_INFO_DATA {
	//     IFLA_MACVLAN_MODE = MACVLAN_MODE_PRIVATE
	//   }
	// }

	Attribute ifname(IFLA_IFNAME, name, std::strlen(name));
	Attribute address(IFLA_ADDRESS, macAddress, 6);
	Attribute operstate(IFLA_OPERSTATE, 6); // IF_OPER_UP
	Attribute link(IFLA_LINK, (std::uint32_t)interface);
	Attribute linkinfo(IFLA_LINKINFO);
	Attribute kind(IFLA_INFO_KIND, "macvlan", 8);
	Attribute data(IFLA_INFO_DATA);
	Attribute mode(IFLA_MACVLAN_MODE, (std::uint32_t)MACVLAN_MODE_PRIVATE);

	
	Attribute attr;
	attr.addAttribute(&ifname);
	attr.addAttribute(&address);
	attr.addAttribute(&operstate);
	attr.addAttribute(&link);
	attr.addAttribute(&linkinfo);
	{
		linkinfo.addAttribute(&kind);
		linkinfo.addAttribute(&data);
		{
			data.addAttribute(&mode);
		}
	}

	std::vector<std::uint8_t> buffer;
	buffer.resize(16 + 16 + attr.effectiveSize());

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer.data());
	hdr->nlmsg_len = buffer.size();
	hdr->nlmsg_type = RTM_NEWLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer.data() + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = ARPHRD_ETHER;
	msg->ifi_index = 0;
	msg->ifi_flags = 0;
	msg->ifi_change = 0;

	attr.toPacket(buffer.data() + 32);

	int newInterface = 0;
	int err = sendNetlinkPacket(buffer.data(), buffer.size(), AF_UNSPEC, 0, &newInterface);
	if (err >= 0)
		return newInterface;
	else
	{
		syslog(LOG_WARNING, "Netlink: Error creating macvlan interface: %s", std::strerror(0 - err));
		return -1;
	}
}

bool Netlink::removeInterface (int interface)
{
	std::uint8_t buffer[16 + 16];

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer);
	hdr->nlmsg_len = sizeof(buffer);
	hdr->nlmsg_type = RTM_DELLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST;
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = ARPHRD_ETHER;
	msg->ifi_index = interface;
	msg->ifi_flags = 0;
	msg->ifi_change = 0;

	syslog(LOG_DEBUG, "Removing interface %i", interface);

	return sendNetlinkPacket(buffer, sizeof(buffer)) >= 0;
}

Netlink::Attribute::Attribute (std::uint16_t type, const void *data, unsigned int size) :
	m_type(type)
{
	if (size > 0)
	{
		m_buffer.resize(size);
		std::memcpy(m_buffer.data(), data, size);
	}
}

Netlink::Attribute::Attribute (std::uint16_t type, std::uint32_t value) :
	m_type(type)
{
	m_buffer.resize(4);
	*reinterpret_cast<std::uint32_t *>(m_buffer.data()) =  value;
}

Netlink::Attribute::Attribute () :
	m_type(0)
{
}

Netlink::Attribute::~Attribute ()
{
}

void Netlink::Attribute::addAttribute (const Attribute *attribute)
{
	m_attributes.push_back(attribute);
}

unsigned int Netlink::Attribute::size () const
{
	unsigned int size = (m_type == 0 ? 0 : 4);
	if (m_attributes.size() > 0)
	{
		for (AttributeList::const_iterator attribute = m_attributes.begin(); attribute != m_attributes.end(); ++attribute)
		{
			size += (*attribute)->effectiveSize();
		}
	}
	else
	{
		size += m_buffer.size();
	}

	return size;
}

unsigned int Netlink::Attribute::effectiveSize () const
{
	unsigned int size = this->size();
	if (size & 0x03)
		size = (size & ~0x03) + 4;
	return size;
}

void Netlink::Attribute::toPacket (void *buffer) const
{
	std::uint8_t *ptr = reinterpret_cast<std::uint8_t *>(buffer);
	if (m_type != 0)
	{
		*reinterpret_cast<std::uint16_t *>(ptr) = size();
		*reinterpret_cast<std::uint16_t *>(ptr + 2) = m_type;
		ptr += 4;
	}
	if (m_attributes.size() > 0)
	{
		for (AttributeList::const_iterator attribute = m_attributes.begin(); attribute != m_attributes.end(); ++attribute)
		{
			(*attribute)->toPacket(ptr);
			ptr += (*attribute)->effectiveSize();
		}
	}
	else if (m_buffer.size() > 0)
	{
		std::memcpy(ptr, m_buffer.data(), m_buffer.size());
		if (m_buffer.size() & 0x03 != 0)
			std::memset(ptr + m_buffer.size(), 0, 4 - (m_buffer.size() & 0x03));
	}
}

bool Netlink::setMac (int interface, const std::uint8_t *macAddress)
{
	Attribute attr(IFLA_ADDRESS, macAddress, 6);

	std::vector<std::uint8_t> buffer;
	buffer.resize(16 + 16 + attr.size());

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer.data());
	hdr->nlmsg_len = buffer.size();
	hdr->nlmsg_type = RTM_SETLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST;
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer.data() + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = ARPHRD_ETHER;
	msg->ifi_index = interface;
	msg->ifi_flags = 0;
	msg->ifi_change = 0;

	attr.toPacket(buffer.data() + 32);

	return sendNetlinkPacket(buffer.data(), buffer.size()) >= 0;
}

bool Netlink::toggleInterface (int interface, bool up)
{
	/*
	std::uint8_t buffer[16 + 16];

	nlmsghdr *hdr = reinterpret_cast<nlmsghdr *>(buffer);
	hdr->nlmsg_len = sizeof(buffer);
	hdr->nlmsg_type = RTM_SETLINK;
	hdr->nlmsg_flags = NLM_F_REQUEST;
	hdr->nlmsg_seq = 1;
	hdr->nlmsg_pid = getpid();

	ifinfomsg *msg = reinterpret_cast<ifinfomsg *>(buffer + 16);
	msg->ifi_family = AF_UNSPEC;
	msg->__ifi_pad = 0;
	msg->ifi_type = ARPHRD_ETHER;
	msg->ifi_index = interface;
	msg->ifi_flags = (up ? IFF_UP : 0);
	msg->ifi_change = IFF_UP;

	return sendNetlinkPacket(buffer, sizeof(buffer)) >= 0;
	*/
	ifreq ifr;
	if_indextoname(interface, ifr.ifr_ifrn.ifrn_name);

	int s = socket(PF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
	{
		syslog(LOG_ERR, "Netlink: Error getting interface flags: %s", std::strerror(errno));
		close(s);
		return false;
	}

	short int newFlags = ifr.ifr_ifru.ifru_flags;
	if (up)
		newFlags |= IFF_UP;
	else
		newFlags &= ~IFF_UP;
	if (newFlags != ifr.ifr_ifru.ifru_flags)
	{
		ifr.ifr_ifru.ifru_flags = newFlags;
		if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
		{
			syslog(LOG_ERR, "Netlink: Error setting interface flags: %s", std::strerror(errno));
			close(s);
			return false;
		}
	}

	close(s);

	return true;
}
