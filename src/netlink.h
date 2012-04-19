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

#ifndef INCLUDE_NETLINK_H
#define INCLUDE_NETLINK_H

#include "util.h"

#include <cstdint>

struct nl_sock;

class Netlink
{
	public:
		static int addMacvlanInterface (int interface, const std::uint8_t *macAddress);
		static bool removeInterface (int interface);
		static bool setMac (int interface, const std::uint8_t *macAddress);
		static bool addIpAddress (int interface, const Addr &addr, int family);
		static bool removeIpAddress (int interface, const Addr &addr, int family);
		static bool toggleInterface (int interface, bool up);

	private:
		static nl_sock *netlinkSocket();

	private:
		static nl_sock *m_socket;
};

#endif // INCLUDE_NETLINK_H
