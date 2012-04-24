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

#ifndef INCLUDE_OPENVRRP_VRRPLISTENER_H
#define INCLUDE_OPENVRRP_VRRPLISTENER_H

#include <cstdint>
#include <map>

class VrrpService;

class VrrpListener
{
	private:
		VrrpListener *getListener (int interface, int family);

		void registerService (VrrpService *service);
		void unregisterService (VrrpService *service);

		int error () const;

	private:
		typedef std::map<std::uint8_t, VrrpService *> ServiceMap;
		typedef std::map<int, VrrpListener *> ListenerMap;

	private:
		VrrpListener (int interface, int family);
		~VrrpListener ();

		void close ();		
		bool onIncomingPacket ();
		static void socketCallback (int fd, void *userData);
	
	private:	
		int m_interface;
		int m_family;
		int m_socket;
		int m_error;
		ServiceMap m_services;
		std::uint8_t m_controlBuffer[512];
		std::uint8_t m_buffer[1500];
		static ListenerMap m_ipv4Listeners;
		static ListenerMap m_ipv6Listeners;

};

#endif // INCLUDE_OPENVRRP_VRRPLISTENER_H
