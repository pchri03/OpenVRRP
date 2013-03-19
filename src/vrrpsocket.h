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

#ifndef INCLUDE_OPENVRRP_VRRPSOCKET_H
#define INCLUDE_OPENVRRP_VRRPSOCKET_H

#include "ipaddress.h"

#include <cstdint>
#include <map>

class VrrpEventListener;

class VrrpSocket
{
	public:
		void addEventListener (unsigned int interface, std::uint_fast8_t virtualRouterId, VrrpEventListener *eventListener);
		void removeEventListener (unsigned int interface, std::uint_fast8_t virtualRouterId);
		bool sendPacket (
				unsigned int interface,
				const IpAddress &address,
				std::uint_fast8_t virtualRouterId,
				std::uint_fast8_t priority,
				std::uint_fast16_t maxAdvertisementInterval,
				const IpAddressSet &addresses);
		
		inline int error () const
		{
			return m_error;
		}

		bool addInterface (int interface);
		bool removeInterface (int interface);

		static VrrpSocket *instance (int family);
		static void cleanup ();

		static std::uint_fast64_t routerChecksumErrors ()
		{
			return m_routerChecksumErrors;
		}

		static std::uint_fast64_t routerVersionErrors ()
		{
			return m_routerVersionErrors;
		}

		static std::uint_fast64_t routerVrIdErrors ()
		{
			return m_routerVrIdErrors;
		}

	private:
		explicit VrrpSocket (int family);
		~VrrpSocket ();

		bool createSocket ();
		void closeSocket ();

		bool onSocketPacket ();

		void decodeControlMessage (const msghdr &hdr, int &interface, IpAddress &address);

		static void socketCallback (int fd, void *userData);

	private:
		typedef std::map<unsigned int, std::map<std::uint_fast8_t, VrrpEventListener *> > EventListenerMap;
		typedef std::map<int, unsigned int> InterfaceMap;

		int m_family;
		int m_error;
		int m_socket;
		const char *m_name;
		IpAddress m_multicastAddress;
		EventListenerMap m_listeners;
		std::uint8_t m_buffer[2048];
		std::uint8_t m_controlBuffer[1024];
		std::map<int,unsigned int> m_interfaceCount;
		static std::uint_fast64_t m_routerChecksumErrors;
		static std::uint_fast64_t m_routerVersionErrors;
		static std::uint_fast64_t m_routerVrIdErrors;

	private:
		static VrrpSocket *m_ipv4Instance;
		static VrrpSocket *m_ipv6Instance;
};

#endif // INCLUDE_OPENVRRP_VRRPSOCKET_H
