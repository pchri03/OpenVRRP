/*
 * Copyright (C) 2013 Peter Christensen <pch@ordbogen.com>
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

#ifndef INCLUDE_OPENVRRP_IPSUBNET_H
#define INCLUDE_OPENVRRP_IPSUBNET_H

#include <string>
#include <list>
#include <set>

#include "ipaddress.h"

class IpSubnet
{
	public:
		IpSubnet (const char *address);
		IpSubnet (const IpAddress &address);
		IpSubnet (const IpAddress &address, unsigned int cidr)
			: m_address(address)
			, m_cidr(cidr)
		{
		}

		IpSubnet ()
			: m_cidr(0)
		{
		}

		~IpSubnet ()
		{
		}

		bool operator< (const IpSubnet &other) const;

		IpAddress address () const
		{
			return m_address;
		}

		unsigned int cidr () const
		{
			return m_cidr;
		}

		void setAddress (const IpAddress &address)
		{
			m_address = address;
		}

		void setCidr (unsigned int cidr)
		{
			m_cidr = cidr;
		}

		std::string toString () const;

	private:
		IpAddress m_address;
		unsigned int m_cidr;
};

typedef std::list<IpSubnet> IpSubnetList;
typedef std::set<IpSubnet> IpSubnetSet;

#endif // INCLUDE_OPENVRRP_IPSUBNET_H

