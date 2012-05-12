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

#include "interface.h"

Interface::Interface (int interface) :
	m_ifindex(interface)
{
}

Interface::Interface (const char *interface) :
	m_ifindex(if_nametoindex(interface))
{
}

Interface::~Interface ()
{
}

IpAddressList Interface::ipAddresses (int family)
{
	// TODO
}

bool Interface::addIpAddress (const IpAddress &address) const
{
	// TODO
}

bool Interface::removeIpAddress (const IpAddress &address) const
{
	// TODO
}

bool Interface::getMac (std::uint8_t *mac) const
{
	// TODO
}

bool Interface::setMac (const std::uint8_t *mac) const
{
	// TODO
}

InterfaceList Interface::virtualInterfaces () const
{
	// TODO
}

Interface Interface::addVirtualInterface (const std::uint8_t mac) const
{
	// TODO
}

void Interface::removeVirtualInterface (const Interface &interface) const
{
}

bool Interface::valid () const
{
	return m_ifindex != 0;
}
