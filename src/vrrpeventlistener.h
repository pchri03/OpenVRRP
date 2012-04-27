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

#ifndef INCLUDE_OPENVRRP_VRRPEVENTLISTENER_H
#define INCLUDE_OPENVRRP_VRRPEVENTLISTENER_H

#include "ipaddress.h"

#include <cstdint>

class VrrpEventListener
{
	public:
		virtual void onIncomingVrrpPacket (
				unsigned int interface,
				const IpAddress &address,
				std::uint_fast8_t virtualRouterId,
				std::uint_fast8_t priority,
				std::uint_fast16_t maxAdvertisementInterval,
				const IpAddressList &addresses);
};

#endif // INCLUDE_OPENVRRP_VRRPEVENTLISTENER_H
