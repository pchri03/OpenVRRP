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
#include "util.h"

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
			// Join multicast address
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

			// Enable reception of packet info (necessary for checksum calculation)
			int val = 1;
			if (setsockopt(m_socket, IPPROTO_IP, IP_PKTINFO, &val, sizeof(val)) == -1)
				syslog(LOG_WARNING, "Error enabling reception of packet info: %s", std::strerror(errno));

			// Enable reception of TTL
			if (setsockopt(m_socket, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val)) == -1)
				syslog(LOG_WARNING, "Error enabling reception of TTL: %s", std::strerror(errno));
		}
		else // if (family == AF_INET6)
		{
			// Disable IPv4
			int val = 1;
			if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) == -1)
				syslog(LOG_WARNING, "Error disabling IPv4 usage on IPv6 socket: %s", std::strerror(errno));

			// Join multicast address
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

			// Enable reception of packet info (necessary for checksum calculation)
			if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_PKTINFO, &val, sizeof(val)) == -1)
				syslog(LOG_WARNING, "Error enabling reception of packet info: %s", std::strerror(errno));

			// Enable reception of hop limit
			if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_HOPLIMIT, &val, sizeof(val)) == -1)
				syslog(LOG_WARNING, "Error enabling reception of hop limit: %s", std::strerror(errno));
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
	m_socket = -1;
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

bool VrrpListener::onIncomingPacket ()
{
	// Prepare data structures
	iovec data;
	data.iov_base = m_buffer;
	data.iov_len = sizeof(m_buffer);

	IpAddress srcAddress;

	msghdr hdr;
	hdr.msg_name = srcAddress.socketAddress();
	hdr.msg_namelen = srcAddress.socketAddressSize();
	hdr.msg_iov = &data;
	hdr.msg_iovlen = 1;
	hdr.msg_control = m_controlBuffer;
	hdr.msg_controllen = sizeof(m_controlBuffer);

	// Receive packet
	ssize_t size;
	while ((size = recvmsg(m_socket, &hdr, MSG_DONTWAIT)) == -1 && errno == EINTR);
	if (size == -1)
	{
		syslog(LOG_ERR, "Error receiving packet: %s", std::strerror(errno));
		return false;
	}

	// Parse through control messages, receiving TTL/HOPLIMIT and destination address
	IpAddress dstAddress;
	std::uint8_t ttl = 255; // Assume TTL 255 if we cannot determine TTL
	const std::uint8_t *ptr = m_controlBuffer;
	unsigned int remaining = hdr.msg_controllen;
	while (remaining > sizeof(cmsghdr))
	{
		const cmsghdr *hdr = reinterpret_cast<const cmsghdr *>(ptr);
		if (hdr->cmsg_len < sizeof(cmsghdr) || hdr->cmsg_len > remaining)
		{
			syslog(LOG_WARNING, "Got invalid control data: %s", std::strerror(errno));
			break;
		}

		if (m_family == AF_INET)
		{
			if (hdr->cmsg_level == IPPROTO_IP)
			{
				if (hdr->cmsg_type == IP_TTL && hdr->cmsg_len >= sizeof(cmsghdr) + 1)
					ttl = ptr[sizeof(cmsghdr)];
				else if (hdr->cmsg_type == IP_PKTINFO && hdr->cmsg_len >= sizeof(cmsghdr) + sizeof(in_pktinfo))
				{
					const in_pktinfo *pktinfo = reinterpret_cast<const in_pktinfo *>(ptr + sizeof(cmsghdr));
					if (pktinfo->ipi_ifindex == m_interface)
						dstAddress = IpAddress(&pktinfo->ipi_addr, AF_INET);
				}
			}
		}
		else if (m_family == AF_INET6)
		{
			if (hdr->cmsg_level == IPPROTO_IPV6)
			{
				if (hdr->cmsg_type == IPV6_HOPLIMIT && hdr->cmsg_len >= sizeof(cmsghdr) + 1)
					ttl = ptr[sizeof(cmsghdr)];
				else if (hdr->cmsg_type == IPV6_PKTINFO && hdr->cmsg_len >= sizeof(cmsghdr) + sizeof(in6_pktinfo))
				{
					const in6_pktinfo *pktinfo = reinterpret_cast<const in6_pktinfo *>(ptr + sizeof(cmsghdr));
					if (pktinfo->ipi6_ifindex == m_interface)
						dstAddress = IpAddress(&pktinfo->ipi6_addr, AF_INET6);
				}
			}
		}

		ptr += hdr->cmsg_len;
		remaining -= hdr->cmsg_len;
	}

	// Verify TTL
	if (ttl != 255)
	{
		syslog(LOG_NOTICE, "Dropped VRRP packet with TTL %hhu", ttl);
		return false;
	}

	// Verify VRRP header
	if (size < 8)
	{
		syslog(LOG_NOTICE, "Dropped VRRP packet smaller than 8 bytes");
		return false;
	}

	// Verify VRRP version and type
	if (m_buffer[0] != 0x31) // VRRPv3 ADVERTISEMENT
	{
		if (m_buffer[0] == 0x21) // VRRPv2 ADVERTISEMENT
			syslog(LOG_NOTICE, "Dropped VRRPv2 packet");
		else
			syslog(LOG_NOTICE, "Dropped unknown VRRP packet");
		return false;
	}

	// Verify VRRP checksum
	if (dstAddress.family() == AF_UNSPEC)
		syslog(LOG_WARNING, "Unable to get destination address. Checksum will not be verified");
	else if (Util::checksum(m_buffer, size, srcAddress, dstAddress) != 0)
	{
		syslog(LOG_NOTICE, "Dropped VRRP packet with invalid checksum");
		return false;
	}

	// Verify VRRP packet size
	std::uint8_t addressCount = m_buffer[3];
	if (size < 8 + addressCount * IpAddress::familySize(m_family))
	{
		syslog(LOG_NOTICE, "Dropped incomplete VRRP packet");
		return false;
	}

	// Signal service
	std::uint8_t virtualRouterId = m_buffer[1];
	ServiceMap::iterator service = m_services.find(virtualRouterId);
	if (service != m_services.end())
	{
		service->second->onIncomingPacket(m_buffer, size);
		return true;
	}

	return false;
}
