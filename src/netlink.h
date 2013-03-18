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

#include "ipaddress.h"
#include "ipsubnet.h"

#include <cstdint>
#include <list>
#include <vector>

class Netlink
{
	public:
		static int addMacvlanInterface (int interface, const std::uint8_t *macAddress, const char *name);
		static bool removeInterface (int interface);
		static bool setMac (int interface, const std::uint8_t *macAddress);

		static IpAddress getPrimaryIpAddress (int interface, int family);

		static bool addIpAddress (int interface, const IpSubnet &ip)
		{
			return modifyIpAddress(interface, ip, true);
		}

		static bool removeIpAddress (int interface, const IpSubnet &ip)
		{
			return modifyIpAddress(interface, ip, false);
		}
		static bool toggleInterface (int interface, bool up);

	private:
		static bool modifyIpAddress (int interface, const IpSubnet &ip, bool add);
		static int sendNetlinkPacket (const void *packet, unsigned int size, int family = AF_UNSPEC, IpAddress *address = 0, int *interface = 0);

		class Attribute
		{
			public:
				Attribute (std::uint16_t type, const void *data = 0, unsigned int size = 0);
				Attribute (std::uint16_t type, std::uint32_t value);
				Attribute ();
				~Attribute ();

				void addAttribute (const Attribute *attribute);

				unsigned int size () const;
				unsigned int effectiveSize () const;
				void toPacket (void *buffer) const;

			private:
				typedef std::list<const Attribute *> AttributeList;
				std::uint16_t m_type;
				std::vector<std::uint8_t> m_buffer;
				AttributeList m_attributes;
		};

};

#endif // INCLUDE_NETLINK_H
