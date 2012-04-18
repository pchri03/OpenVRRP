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

class Netlink
{
	public:
		static int addMacvlanInterface (const char *interface, const std::uint8_t *macAddress);
		static int addMacvlanInterface (int interface, const std::uint8_t *macAddress);
		static bool removeMacvlanInterface (const char *interface);
		static bool removeMacvlanInterface (int interface);
		static bool addIpAddress (const char *interface, const Addr &addr);
		static bool addIpAddress (int interface, const Addr &addr);
		static bool removeIpAddress (const char *interface, const Addr &addr);
		static bool removeIpAddress (int interface, const Addr &addr);
};

#endif // INCLUDE_NETLINK_H
