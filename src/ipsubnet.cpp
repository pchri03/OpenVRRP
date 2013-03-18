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

#include "ipsubnet.h"

#include <cstring>
#include <sstream>

IpSubnet::IpSubnet (const char *address)
{
	const char *ptr = std::strchr(address, '/');
	if (ptr == 0)
	{
		m_address = IpAddress(address);
		if (m_address.family() == AF_INET)
			m_cidr = 32;
		else if (m_address.family() == AF_INET6)
			m_cidr = 128;
		else
			m_cidr = 0;
	}
	else
	{
		std::ptrdiff_t size = ptr - address;
		char *tmp = new char[size + 1];
		std::memcpy(tmp, address, size);
		tmp[size] = 0;

		m_address = IpAddress(tmp);

		delete[] tmp;

		m_cidr = (unsigned int)std::atoi(ptr + 1);
	}
}

IpSubnet::IpSubnet (const IpAddress &address)
	: m_address(address)
{
	if (address.family() == AF_INET)
		m_cidr = 32;
	else if (address.family() == AF_INET6)
		m_cidr = 128;
	else
		m_cidr = 0;
}

std::string IpSubnet::toString () const
{
	std::stringstream str(std::ios_base::out);
	str << m_address.toString() << '/' << m_cidr;
	return str.str();
}
