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

#ifndef INCLUDE_INTERFACE_H
#define INCLUDE_INTERFACE_H

#include "ipaddress.h"

#include <vector>
#include <cstdint>

class Interface
{
	public:
		explicit Interface (int interface);
		explicit Interface (const char *interface);
		~Interface ();

		IpAddressList ipAddresses (int family = AF_UNSPEC) const;
		bool addIpAddress (const IpAddress &address) const;
		bool removeIpAddress (const IpAddress &address) const;

		bool getMac (std::uint8_t *mac) const;
		bool setMac (const std::uint8_t *mac) const;

		InterfaceList virtualInterfaces () const;
		Interface addVirtualInterface (const std::uint8_t *mac) const;
		void removeVirtualInterface (const Interface &interface) const;

		int flags () const;
		void setFlags (int flags, int mask) const

		bool valid () const;

	private:
		int m_ifindex;
};

typedef std::vector<Interface> InterfaceList;

#endif // INCLUDE_INTERFACE_H
