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

#ifndef INCLUDE_OPENVRRP_IP_H
#define INCLUDE_OPENVRRP_IP_H

#include <string>
#include <cstdint>

#include <netinet/in.h>

class Ip
{
	public:
		Ip (const char *address);
		Ip (const void *data, int family);
		Ip (const Ip &other);
		Ip ();

		~Ip ()
		{
		}

		bool operator < (const Ip &other) const;
		bool operator == (const Ip &other) const;
		bool operator != (const Ip &other) const
		{
			return !(*this == other);
		}

		unsigned int size () const
		{
			return familySize(m_family);
		}

		const void *data () const
		{
			return m_buffer;
		}

		int family () const
		{
			return m_family;
		}

		std::string toString () const;

	private:
		static unsigned int familySize (int family)
		{
			if (family == AF_INET)
				return 4;
			else if (family == AF_INET6)
				return 16;
			else
				return 0;
		}
		
	private:
		int m_family;
		std::uint8_t m_buffer[16];
};



#endif // INCLUDE_OPENVRRP_IP_H
