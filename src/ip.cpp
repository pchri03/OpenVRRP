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

#include "ip.h"

#include <cstring>

#include <arpa/inet.h>

Ip::Ip (const char *address)
{
	if (inet_pton(AF_INET6, address, m_buffer) == 1)
		m_family = AF_INET6;
	else if (inet_pton(AF_INET, address, m_buffer) == 1)
		m_family = AF_INET;
	else
		m_family = 0;
}

Ip::Ip (const void *data, int family) :
	m_family(family)
{
	std::memcpy(m_buffer, data, familySize(m_family));
}

Ip::Ip (const Ip &other)
{
	if (other != *this)
	{
		m_family = other.m_family;
		std::memcpy(m_buffer, other.m_buffer, other.size());
	}
}

Ip::Ip () :
	m_family(0)
{
}

bool Ip::operator < (const Ip &other) const
{
	if (family() < other.family())
		return true;
	else if (family() > other.family())
		return false;
	else // if (family() == other.family())
	{
		unsigned int size = this->size();
		for (unsigned int i = 0; i != size; ++i)
		{
			if (m_buffer[i] < other.m_buffer[i])
				return true;
			else if (m_buffer[i] > other.m_buffer[i])
				return false;
		}
		return false;
	}
}

bool Ip::operator == (const Ip &other) const
{
	return family() == other.family() && std::memcmp(m_buffer, other.m_buffer, size());
}

std::string Ip::toString () const
{
	if (m_family == 0)
		return std::string();
	
	char buffer[40];
	inet_ntop(m_family, m_buffer, buffer, sizeof(buffer));
	return std::string(buffer);
}
