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

#ifndef INCLUDE_OPENVRRP_IPADDRESS_H
#define INCLUDE_OPENVRRP_IPADDRESS_H

#include <string>
#include <cstdint>

#include <netinet/in.h>

class IpAddress
{
	public:
		IpAddress (const char *address);
		IpAddress (const void *data, int family);
		IpAddress ();

		~IpAddress ()
		{
		}

		bool operator < (const IpAddress &other) const;
		bool operator == (const IpAddress &other) const;
		bool operator != (const IpAddress &other) const
		{
			return !(*this == other);
		}

		unsigned int size () const
		{
			return familySize(m_addr.common.sa_family);
		}

		const void *data () const
		{
			if (m_addr.common.sa_family == AF_INET)
				return &m_addr.ipv4.sin_addr;
			else if (m_addr.common.sa_family == AF_INET6)
				return &m_addr.ipv6.sin6_addr;
			else
				return 0;
		}

		int family () const
		{
			return m_addr.common.sa_family;
		}

		std::string toString () const;

		socklen_t socketAddressSize () const;
		const sockaddr *socketAddress () const;
		sockaddr *socketAddress ();

		static unsigned int familySize (int family)
		{
			if (family == AF_INET)
				return 4;
			else if (family == AF_INET6)
				return 16;
			else
				return 0;
		}
		
	private:
		union
		{
			sockaddr common;
			sockaddr_in ipv4;
			sockaddr_in6 ipv6;
		} m_addr;
};



#endif // INCLUDE_OPENVRRP_IP_H
