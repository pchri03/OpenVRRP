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

#ifndef INCLUDE_OPENVRRP_VRRPMANAGER_H
#define INCLUDE_OPENVRRP_VRRPMANAGER_H

#include "configuration.h"

#include <map>
#include <cstdint>

class VrrpService;

class VrrpManager
{
	public:
		typedef std::map<int, std::map<std::uint_least8_t, std::map<int, VrrpService *> > > VrrpServiceMap;

		static inline const VrrpServiceMap &services ()
		{
			return m_services;
		}

		static VrrpService *getService (int interface, std::uint_least8_t virtualRouterId, int family, bool createIfMissing = false);
		static void removeService (int interface, std::uint_least8_t virtualRouterId, int family);
		static void removeService (VrrpService *service);

		static bool setup (const Configuration &config);

		static void cleanup ();

		enum ProtocolErrorReason
		{
			NoError = 0,
			IpTtlError = 1,
			VersionError = 2,
			ChecksumError = 3,
			VrIdError = 4
		};

		static void onProtocolError (ProtocolErrorReason error);

	private:
		static VrrpServiceMap m_services;
};

#endif // INCLUDE_OPENVRRP_VRRPMANAGER_H
