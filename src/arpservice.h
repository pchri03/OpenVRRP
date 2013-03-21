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

#include <cstdint>
#include <map>

class ArpService
{
	public:
		static bool addFakeArp (int interface, const IpAddress &address, const std::uint8_t *mac);
		static bool removeFakeArp (int interface, const IpAddress &address);

	private:
		ArpService (int interface);
		~ArpService ();

		static void socketCallback (int fd, void *userData);
		
		void onArpPacket ();

	private:
		typedef std::map<int,ArpService *> ServiceMap;
		static ServiceMap services;

	private:
		typedef std::map<IpAddress, std::uint8_t *> IpAddressMap;
		int m_socket;
		IpAddressMap m_addresses;
};

