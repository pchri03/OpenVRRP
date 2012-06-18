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

#include "util.h"
#include "ipaddress.h"

#include <cstring>

#include <arpa/inet.h>

std::uint16_t Util::checksum (const void *packet, unsigned int size, const IpAddress &srcAddr, const IpAddress &dstAddr, int family)
{
	std::uint8_t pseudoHeader[40];
	unsigned int pseudoHeaderSize;
	if (srcAddr.family() == AF_INET && dstAddr.family() == AF_INET)
	{
		std::memcpy(pseudoHeader, srcAddr.data(), 4);
		std::memcpy(pseudoHeader + 4, dstAddr.data(), 4);
		pseudoHeader[8] = 0;
		pseudoHeader[9] = family;
		*reinterpret_cast<std::uint16_t *>(pseudoHeader + 10) = htons(size);
		pseudoHeaderSize = 12;
	}
	else if (srcAddr.family() == AF_INET6 && dstAddr.family() == AF_INET6)
	{
		std::memcpy(pseudoHeader, srcAddr.data(), 16);
		std::memcpy(pseudoHeader + 16, dstAddr.data(), 16);
		*reinterpret_cast<std::uint32_t *>(pseudoHeader + 32) = htonl(size);
		*reinterpret_cast<std::uint32_t *>(pseudoHeader + 36) = htonl(family);
		pseudoHeaderSize = 40;
	}
	else
		return 0;

	std::uint32_t sum = 0;

	// Checksum pseudo header
	const std::uint16_t *ptr = reinterpret_cast<const std::uint16_t *>(pseudoHeader);
	for (unsigned int i = 0; i != pseudoHeaderSize / 2; ++i, ++ptr)
		sum += ntohs(*ptr);

	// Checksum packet
	ptr = reinterpret_cast<const std::uint16_t *>(packet);
	for (unsigned int i = 0; i != size / 2; ++i, ++ptr)
		sum += ntohs(*ptr);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	sum = (~sum & 0xFFFF);

	return htons(sum & 0xFFFF);
}
