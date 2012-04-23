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

#include "vrrplistener.h"
#include "ipaddress.h"
#include "mainloop.h"
#include "vrrpservice.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

VrrpListener::ListenerMap VrrpListener::m_ipv4Listeners;
VrrpListener::ListenerMap VrrpListener::m_ipv6Listeners;

VrrpListener *VrrpListener::getListener (int interface, int family)
{
	VrrpListener *listener;
	if (family == AF_INET)
	{
		ListenerMap::iterator it = m_ipv4Listeners.find(interface);
		if (it == m_ipv4Listeners.end())
		{
			listener = new VrrpListener(interface, AF_INET);
			m_ipv4Listeners[interface] = listener;
		}
		else
			listener = it->second;
	}
	else if (family == AF_INET6)
	{
		ListenerMap::iterator it = m_ipv6Listeners.find(interface);
		if (it == m_ipv6Listeners.end())
		{
			listener = new VrrpListener(interface, AF_INET6);
			m_ipv6Listeners[interface] = listener;
		}
		else
			listener = it->second;
	}
	else
		listener = 0;

	return listener;
}

VrrpListener::VrrpListener (int interface, int family) :
	m_interface(interface),
	m_family(family),
	m_socket(socket(family, SOCK_RAW, 112)),
	m_error(0)
{
	if (m_socket == -1)
	{
		m_error = errno;
		syslog(LOG_ERR, "Error creating VRRP socket: %s", std::strerror(m_error));
	}
	else
	{
		// Join multicast
		if (family == AF_INET)
		{
			ip_mreqn req;
			req.imr_multiaddr.s_addr = htonl(0xE0000012);
			req.imr_address.s_addr = INADDR_ANY; // TODO
			req.imr_ifindex = m_interface;

			if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) == -1)
			{
				m_error = errno;
				syslog(LOG_ERR, "Error joining multicast group: %s", std::strerror(m_error));
				close();
				return;
			}
		}
		else // if (family == AF_INET6)
		{
			ipv6_mreq req;
			std::memcpy(&req.ipv6mr_multiaddr, IpAddress("FF12::12").data(), 16);
			req.ipv6mr_interface = m_interface;

			if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &req, sizeof(req)) == -1)
			{
				m_error = errno;
				syslog(LOG_ERR, "Error joining multicast group: %s", std::strerror(m_error));
				close();
				return;
			}
		}

		MainLoop::addMonitor(m_socket, socketCallback, this);
	}
}

VrrpListener::~VrrpListener ()
{
	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		close();
	}
}

void VrrpListener::close ()
{
	while (::close(m_socket) == -1 && errno == EINTR);
}

void VrrpListener::registerService (VrrpService *service)
{
	m_services[service->virtualRouterId()] = service;
}

void VrrpListener::unregisterService (VrrpService *service)
{
	ServiceMap::iterator it = m_services.find(service->virtualRouterId());
	if (it != m_services.end() && it->second == service)
		m_services.erase(it);
}

int VrrpListener::error () const
{
	return m_error;
}

void VrrpListener::socketCallback (int, void *userData)
{
	VrrpListener *self = reinterpret_cast<VrrpListener *>(userData);
	self->onIncomingPacket();
}

void VrrpListener::onIncomingPacket ()
{
	/* TODO
	   Fetch packet
	   Verify TTL
	   Verity checksum
	   Invoke proper callback
	 */
}
