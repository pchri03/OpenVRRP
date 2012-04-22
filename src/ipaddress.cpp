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

#include "ipaddress.h"

#include <cstring>

#include <arpa/inet.h>

IpAddress::IpAddress (const char *address)
{
	std::memset(&m_addr, 0, sizeof(m_addr));

	std::uint8_t buffer[16];
	if (inet_pton(AF_INET6, address, buffer) == 1)
	{
		m_addr.ipv6.sin6_family = AF_INET6;
		std::memcpy(&m_addr.ipv6.sin6_addr, buffer, 16);
	}
	else if (inet_pton(AF_INET, address, buffer) == 1)
	{
		m_addr.ipv4.sin_family = AF_INET;
		m_addr.ipv4.sin_addr.s_addr = *reinterpret_cast<const std::uint32_t *>(buffer);
	}
	else
		m_addr.common.sa_family = AF_UNSPEC;
}

IpAddress::IpAddress (const void *data, int family)
{
	std::memset(&m_addr, 0, sizeof(m_addr));
	m_addr.common.sa_family = family;
	if (family == AF_INET)
		m_addr.ipv4.sin_addr.s_addr = *reinterpret_cast<const std::uint32_t *>(data);
	else
		std::memcpy(&m_addr.ipv6.sin6_addr, data, 16);
}

IpAddress::IpAddress ()
{
	m_addr.common.sa_family = AF_UNSPEC;
}

bool IpAddress::operator < (const IpAddress &other) const
{
	if (family() < other.family())
		return true;
	else if (family() > other.family())
		return false;
	else // if (family() == other.family())
		return std::memcmp(data(), other.data(), size()) < 0;
}

bool IpAddress::operator == (const IpAddress &other) const
{
	return family() == other.family() && std::memcmp(data(), other.data(), size());
}

std::string IpAddress::toString () const
{
	if (m_addr.common.sa_family == AF_UNSPEC)
		return std::string();
	
	char buffer[40];
	inet_ntop(m_addr.common.sa_family, data(), buffer, sizeof(buffer));
	return std::string(buffer);
}

socklen_t IpAddress::socketAddressSize () const
{
	if (m_addr.common.sa_family == AF_INET)
		return sizeof(m_addr.ipv4);
	else if (m_addr.common.sa_family == AF_INET6)
		return sizeof(m_addr.ipv6);
	else
		return sizeof(m_addr);
}

const sockaddr *IpAddress::socketAddress () const
{
	return &m_addr.common;
}

sockaddr *IpAddress::socketAddress ()
{
	return &m_addr.common;
}
