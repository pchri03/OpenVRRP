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

#include "mainloop.h"
#include "util.h"
#include "vrrpeventlistener.h"
#include "vrrpsocket.h"
#include "vrrpmanager.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <sys/socket.h>

VrrpSocket *VrrpSocket::m_ipv4Instance = 0;
VrrpSocket *VrrpSocket::m_ipv6Instance = 0;

std::uint_fast64_t VrrpSocket::m_routerVersionErrors = 0;
std::uint_fast64_t VrrpSocket::m_routerChecksumErrors = 0;
std::uint_fast64_t VrrpSocket::m_routerVrIdErrors = 0;

VrrpSocket::VrrpSocket (int family) :
	m_family(family),
	m_error(0),
	m_socket(-1)
{
	if (m_family == AF_INET)
	{
		m_multicastAddress = IpAddress("224.0.0.18");
		m_name = "VRRP IPv4";
	}
	else // if (m_family == AF_INET6)
	{
		m_multicastAddress = IpAddress("FF12::12");
		m_name = "VRRP IPv6";
	}

	if (createSocket())
		MainLoop::addMonitor(m_socket, socketCallback, this);
	else
		closeSocket();
}

VrrpSocket::~VrrpSocket ()
{
	closeSocket();
}

bool VrrpSocket::createSocket ()
{
	m_socket = socket(m_family, SOCK_RAW | SOCK_CLOEXEC, 112);
	if (m_socket == -1)
	{
		m_error = errno;
		syslog(LOG_ERR, "%s: Error creating socket: %s", m_name, std::strerror(m_error));
		return false;
	}

	if (m_family == AF_INET)
	{
		int val = 0;

		val = 0;
		if (setsockopt(m_socket, SOL_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error disabling multicast loopback: %s", m_name, std::strerror(m_error));
			return false;
		}

		val = 255;
		if (setsockopt(m_socket, SOL_IP, IP_MULTICAST_TTL, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error setting multicast TTL: %s", m_name, std::strerror(m_error));
			return false;
		}

		val = 1;
		if (setsockopt(m_socket, SOL_IP, IP_PKTINFO, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error enabling reception of packet info: %s", m_name, std::strerror(m_error));
			return false;
		}
	}
	else // if (m_family == AF_INET6)
	{
		int val;

		val = 1;
		if (setsockopt(m_socket, SOL_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error enabling reception of packet info: %s", m_name, std::strerror(m_error));
			return false;
		}

		val = 0;
		if (setsockopt(m_socket, SOL_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error disabling multicast loopback: %s", m_name, std::strerror(m_error));
			return false;
		}

		val = 255;
		if (setsockopt(m_socket, SOL_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error setting multicast hop limit: %s", m_name, std::strerror(m_error));
			return false;
		}
	}

	return true;
}

void VrrpSocket::closeSocket ()
{
	if (m_socket != -1)
	{
		while (close(m_socket) == -1 && errno == EINTR);
		m_socket = -1;
	}
}

bool VrrpSocket::addInterface (int interface)
{
	InterfaceMap::iterator it = m_interfaceCount.find(interface);
	if (it != m_interfaceCount.end())
		++it->second;
	else if (m_family == AF_INET)
	{
		ip_mreqn req;
		req.imr_multiaddr.s_addr = *reinterpret_cast<const std::uint32_t *>(m_multicastAddress.data());
		req.imr_address.s_addr = INADDR_ANY;
		req.imr_ifindex = interface;
		if (setsockopt(m_socket, SOL_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error joining multicast address: %s", m_name, std::strerror(m_error));
			return false;
		}
		else
			m_interfaceCount[interface] = 1;
	}
	else // if (m_family == AF_INET6)
	{
		ipv6_mreq req;
		std::memcpy(&req.ipv6mr_multiaddr, m_multicastAddress.data(), 16);
		req.ipv6mr_interface = interface;
		if (setsockopt(m_socket, SOL_IPV6, IPV6_ADD_MEMBERSHIP, &req, sizeof(req)) == -1)
		{
			m_error = errno;
			syslog(LOG_ERR, "%s: Error joining multicast address: %s", m_name, std::strerror(m_error));
			return false;
		}
		else
			m_interfaceCount[interface] = 1;
	}

	return true;
}

bool VrrpSocket::removeInterface (int interface)
{
	InterfaceMap::iterator it = m_interfaceCount.find(interface);
	if (it == m_interfaceCount.end())
		return false;
	else if (it->second == 1)
	{
		if (m_family == AF_INET)
		{
			ip_mreqn req;
			req.imr_multiaddr.s_addr = *reinterpret_cast<const std::uint32_t *>(m_multicastAddress.data());
			req.imr_address.s_addr = INADDR_ANY;
			req.imr_ifindex = interface;
			if (setsockopt(m_socket, SOL_IP, IP_DROP_MEMBERSHIP, &req, sizeof(req)) == -1)
			{
				m_error = errno;
				syslog(LOG_ERR, "%s: Error joining multicast address: %s", m_name, std::strerror(m_error));
				return false;
			}
		}
		else // if (m_family == AF_INET6)
		{
			ipv6_mreq req;
			std::memcpy(&req.ipv6mr_multiaddr, m_multicastAddress.data(), 16);
			req.ipv6mr_interface = interface;
			if (setsockopt(m_socket, SOL_IPV6, IPV6_DROP_MEMBERSHIP, &req, sizeof(req)) == -1)
			{
				m_error = errno;
				syslog(LOG_ERR, "%s: Error joining multicast address: %s", m_name, std::strerror(m_error));
				return false;
			}
		}
		m_interfaceCount.erase(it);
		return true;
	}
	else
	{
		--it->second;
		return true;
	}
}

void VrrpSocket::addEventListener (unsigned int interface, std::uint_fast8_t virtualRouterId, VrrpEventListener *eventListener)
{
	m_listeners[interface][virtualRouterId] = eventListener;
}

void VrrpSocket::removeEventListener (unsigned int interface, std::uint_fast8_t virtualRouterId)
{
	EventListenerMap::iterator interfaceListenerMap = m_listeners.find(interface);
	if (interfaceListenerMap != m_listeners.end())
	{
		EventListenerMap::mapped_type::iterator listener = interfaceListenerMap->second.find(virtualRouterId);
		if (listener != interfaceListenerMap->second.end())
			interfaceListenerMap->second.erase(listener);
		if (interfaceListenerMap->second.size() == 0)
			m_listeners.erase(interfaceListenerMap);
	}
}

VrrpSocket *VrrpSocket::instance (int family)
{
	VrrpSocket **ptr;

	if (family == AF_INET)
		ptr = &m_ipv4Instance;
	else if (family == AF_INET6)
		ptr = &m_ipv6Instance;
	else
		return 0;

	if (*ptr == 0)
	{
		VrrpSocket *socket = new VrrpSocket(family);
		if (socket->error() != 0)
			delete socket;
		else
			*ptr = socket;
	}
	return *ptr;
}

void VrrpSocket::cleanup ()
{
	if (m_ipv4Instance != 0)
	{
		delete m_ipv4Instance;
		m_ipv4Instance = 0;
	}
	if (m_ipv6Instance != 0)
	{
		delete m_ipv6Instance;
		m_ipv6Instance = 0;
	}
}

void VrrpSocket::socketCallback (int, void *userData)
{
	VrrpSocket *socket = reinterpret_cast<VrrpSocket *>(userData);
	socket->onSocketPacket();
}

bool VrrpSocket::onSocketPacket ()
{
	// Prepare data structures
	iovec iov;
	iov.iov_base = m_buffer;
	iov.iov_len = sizeof(m_buffer);

	IpAddress srcAddress;

	msghdr hdr;
	hdr.msg_name = srcAddress.socketAddress();
	hdr.msg_namelen = srcAddress.socketAddressSize();
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = m_controlBuffer;
	hdr.msg_controllen = sizeof(m_controlBuffer);

	// Receive packet
	ssize_t size;
	while ((size = recvmsg(m_socket, &hdr, MSG_DONTWAIT)) == -1 && errno == EINTR);
	if (size == -1)
	{
		m_error = errno;
		syslog(LOG_WARNING, "%s: Error receiving packet: %s", m_name, std::strerror(m_error));
		return false;
	}

	// Parse through control message, receiving TTL/HOPLIMIT, destination address and source interface
	int interface = 0;
	IpAddress dstAddress;
	decodeControlMessage(hdr, interface, dstAddress);

	// Find event listener list for interface
	EventListenerMap::const_iterator interfaceListenerMap = m_listeners.find(interface);
	if (interfaceListenerMap == m_listeners.end())
		return false;

	const std::uint8_t *packet;
	std::uint8_t ttl;
	if (m_family == AF_INET)
	{
		const iphdr *ip = reinterpret_cast<const iphdr *>(m_buffer);
		packet = m_buffer + ip->ihl * 4;
		size = ntohs(ip->tot_len) - ip->ihl * 4;
		ttl = ip->ttl;
	}
	else // if (m_family == AF_INET6)
	{
		const ip6_hdr *ip = reinterpret_cast<const ip6_hdr *>(m_buffer);
		size = ntohs(ip->ip6_plen);
		ttl = ip->ip6_hops;
		if (ip->ip6_nxt == 112) // VRRP
			packet = m_buffer + sizeof(ip6_hdr);
		else
		{
			const ip6_ext *ext = reinterpret_cast<const ip6_ext *>(packet);
			while (ext->ip6e_nxt != 112)
			{
				packet += ext->ip6e_len + 2;
				size -= ext->ip6e_len + 2;
				ext = reinterpret_cast<const ip6_ext *>(packet);
			}
		}
	}

	// Verify VRRP header
	if (size < 8)
	{
		syslog(LOG_NOTICE, "%s: Discarded VRRP packet smaller than 8 bytes", m_name);

		// Since packet is too small, we cannot know the router id for sure. VRRPV3-MIB requires us to update vrrpv3StatisticsPacketLengthError, so we'll notify all services
		for (EventListenerMap::mapped_type::const_iterator listener = interfaceListenerMap->second.begin(); listener != interfaceListenerMap->second.end(); ++listener)
		{
			listener->second->onIncomingVrrpError(interface, listener->first, VrrpEventListener::PacketLengthError);
		}
		return false;
	}

	// Verify VRRP version
	if ((packet[0] & 0xF0) != 0x30)
	{
		syslog(LOG_NOTICE, "%s: Discarded unknown VRRP packet", m_name);

		if (packet[0] == 0x21)
		{
			// Packet is VRRPv2 - extract virtual router id and attempt to notify the service
			std::uint_fast8_t virtualRouterId = packet[1];

			// TODO - Verify checksum 

			EventListenerMap::mapped_type::const_iterator listener = interfaceListenerMap->second.find(virtualRouterId);
			if (listener != interfaceListenerMap->second.end())
				listener->second->onIncomingVrrpError(interface, listener->first, VrrpEventListener::VersionError);
		}

		// Also increment global statistics
		++m_routerVersionErrors;
		return false;
	}

	// Verify VRRP checksum
	if (dstAddress.family() == AF_UNSPEC)
		syslog(LOG_WARNING, "%s: Unable to get destination address. Checksum will not be verified", m_name);
	else if (Util::checksum(packet, size, srcAddress, dstAddress, 112) != 0)
	{
		syslog(LOG_NOTICE, "%s: Discarded VRRP packet with invalid checksum", m_name);

		// Increment global statistics
		++m_routerChecksumErrors;
		return false;
	}

	// Verify VRRP type
	if ((packet[0] & 0x0F) != 0x01)
	{
		syslog(LOG_NOTICE, "%s: Discarded VRRP packet with unknown type", m_name);
		
		// Since packet type is wrong, we cannot know the router id for sure. VRRPV3-MIB requires us to update vrrpv3StatisticsRcvdInvalidTypePackets, so we'll notify all services
		for (EventListenerMap::mapped_type::const_iterator listener = interfaceListenerMap->second.begin(); listener != interfaceListenerMap->second.end(); ++listener)
		{
			listener->second->onIncomingVrrpError(interface, listener->first, VrrpEventListener::InvalidTypeError);
		}

		return false;
	}

	// Get VRRP parameters
	std::uint_fast8_t virtualRouterId = packet[1];
	std::uint_fast8_t priority = packet[2];
	std::uint_fast8_t addressCount = packet[3];
	std::uint_fast16_t maxAdvertisementInterval = ((std::uint_fast16_t)(packet[4] & 0x0F) << 8) | packet[5];

	// Find event listener for virtual router id
	EventListenerMap::mapped_type::const_iterator listener = interfaceListenerMap->second.find(virtualRouterId);
	if (listener == interfaceListenerMap->second.end())
	{
		// We are not associated with the virtual router id in mind, but VRRPV3-MIB requires us to set vrrpv3StatisticsProtoErrReason to VrIdError, so we'll notify all services
		for (EventListenerMap::mapped_type::const_iterator listener = interfaceListenerMap->second.begin(); listener != interfaceListenerMap->second.end(); ++listener)
		{
			listener->second->onIncomingVrrpError(interface, listener->first, VrrpEventListener::VrIdError);
		}

		++m_routerVrIdErrors;
		return false;
	}

	// Verify TTL
	if (ttl != 255)
	{
		syslog(LOG_NOTICE, "%s: Discarded VRRP packet with TTL %hhu", m_name, ttl);

		// VRRPV3-MIB requires us to update vrrpv3StatisticsIpTtlErrors and set vrrpv3StatisticsProtoErrReason to VrId, so we'll notify the service
		listener->second->onIncomingVrrpError(interface, virtualRouterId, VrrpEventListener::IpTtlError);
		return false;
	}

	// Verify VRRP packet size
	unsigned int addressSize = IpAddress::familySize(m_family);
	if (size < 8 + addressCount * addressSize)
	{
		syslog(LOG_NOTICE, "%s: Discarded incomplete VRRP packet", m_name);

		// VRRPV3-MIB specifies vrrpv3StatisticsPacketLengthErrors to be the number of packets received less than the length of the VRRP header, but it makes more sense to
		// register all packets with invalid lengths, so we'll notify the service
		listener->second->onIncomingVrrpError(interface, virtualRouterId, VrrpEventListener::PacketLengthError); // Expected to increment vrrpv3StatisticsPacketLengthErrors
		return false;
	}

	// Create address list
	IpAddressList addresses;
	const std::uint8_t *ptr = packet + 8;
	for (std::uint_fast8_t i = 0; i != addressCount; ++i, ptr += addressSize)
		addresses.push_back(IpAddress(ptr, m_family));

	// Call event listener
	listener->second->onIncomingVrrpPacket(
			interface,
			srcAddress,
			virtualRouterId,
			priority,
			maxAdvertisementInterval,
			addresses);

	return true;
}

void VrrpSocket::decodeControlMessage (const msghdr &hdr, int &interface, IpAddress &address)
{
	const std::uint8_t *ptr = reinterpret_cast<const std::uint8_t *>(hdr.msg_control);
	unsigned int remaining = hdr.msg_controllen;
	while (remaining >= sizeof(cmsghdr))
	{
		const cmsghdr *hdr = reinterpret_cast<const cmsghdr *>(ptr);
		if (hdr->cmsg_len < sizeof(cmsghdr) || hdr->cmsg_len > remaining)
		{
			syslog(LOG_WARNING, "%s: Got invalid control data: %s", m_name, std::strerror(m_error));
			break;
		}

		if (m_family == AF_INET && hdr->cmsg_level == SOL_IP && hdr->cmsg_type == IP_PKTINFO && hdr->cmsg_len >= sizeof(cmsghdr) + sizeof(in_pktinfo))
		{
			const in_pktinfo *pktinfo = reinterpret_cast<const in_pktinfo *>(ptr + sizeof(cmsghdr));
			interface = pktinfo->ipi_ifindex;
			address = IpAddress(&pktinfo->ipi_addr, AF_INET);
		}
		else if (m_family == AF_INET6 && hdr->cmsg_level == SOL_IPV6 && hdr->cmsg_type == IPV6_PKTINFO && hdr->cmsg_len >= sizeof(cmsghdr) + sizeof(in_pktinfo))
		{
			const in6_pktinfo *pktinfo = reinterpret_cast<const in6_pktinfo *>(ptr + sizeof(cmsghdr));
			interface = pktinfo->ipi6_ifindex;
			address = IpAddress(&pktinfo->ipi6_addr, AF_INET6);
		}

		remaining -= hdr->cmsg_len;
	}
}

bool VrrpSocket::sendPacket (
		unsigned int interface,
		const IpAddress &address,
		std::uint_fast8_t virtualRouterId,
		std::uint_fast8_t priority,
		std::uint_fast16_t maxAdvertisementInterval,
		const IpAddressSet &addresses)
{
	// Sanity
	if (address.family() != m_family)
		return false;

	// Create VRRP header
	m_buffer[0] = 0x31; // VRRPv3 ADVERTISEMENT
	m_buffer[1] = virtualRouterId;
	m_buffer[2] = priority;
	m_buffer[3] = addresses.size();
	m_buffer[4] = (maxAdvertisementInterval >> 8) & 0x0F;
	m_buffer[5] = (maxAdvertisementInterval & 0xFF);
	m_buffer[6] = 0; // Checksum
	m_buffer[7] = 0; // Checksum

	// Add IP addresses
	std::uint8_t *ptr = m_buffer + 8;
	unsigned int addressSize = IpAddress::familySize(m_family);
	for (IpAddressSet::const_iterator it = addresses.begin(); it != addresses.end(); ++it, ptr += addressSize)
	{
		if (it->family() != m_family)
			return false;
		std::memcpy(ptr, it->data(), addressSize);
	}

	// Calculate checksum
	unsigned int packetSize = 8 + addresses.size() * addressSize;
	*reinterpret_cast<std::uint16_t *>(m_buffer + 6) = Util::checksum(m_buffer, packetSize, address, m_multicastAddress, 112);

	// Fill control structure
	unsigned int controlSize;
	if (m_family == AF_INET)
	{
		cmsghdr *cmsg = reinterpret_cast<cmsghdr *>(m_controlBuffer);
		cmsg->cmsg_len = sizeof(cmsghdr) + sizeof(in_pktinfo);
		cmsg->cmsg_level = SOL_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		
		in_pktinfo *pktinfo = reinterpret_cast<in_pktinfo *>(m_controlBuffer + sizeof(cmsghdr));
		pktinfo->ipi_ifindex = interface;
		std::memcpy(&pktinfo->ipi_spec_dst, address.data(), address.size());
		std::memcpy(&pktinfo->ipi_addr, m_multicastAddress.data(), address.size());

		controlSize = cmsg->cmsg_len;
	}
	else // if (m_family == AF_INET6)
	{
		cmsghdr *cmsg = reinterpret_cast<cmsghdr *>(m_controlBuffer);
		cmsg->cmsg_len = sizeof(cmsghdr) + sizeof(in6_pktinfo);
		cmsg->cmsg_level = SOL_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;

		in6_pktinfo *pktinfo = reinterpret_cast<in6_pktinfo *>(m_controlBuffer + sizeof(cmsghdr));
		std::memcpy(&pktinfo->ipi6_addr, address.data(), address.size());
		pktinfo->ipi6_ifindex = interface;

		controlSize = cmsg->cmsg_len;
	}

	// Prepare data structures
	iovec iov;
	iov.iov_base = m_buffer;
	iov.iov_len = packetSize;

	msghdr hdr;
	hdr.msg_name = m_multicastAddress.socketAddress();
	hdr.msg_namelen = m_multicastAddress.socketAddressSize();
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = m_controlBuffer;
	hdr.msg_controllen = controlSize;
	hdr.msg_flags = 0;
	
	if (sendmsg(m_socket, &hdr, MSG_DONTWAIT) == -1)
	{
		m_error = errno;
		syslog(LOG_WARNING, "%s: Error sending packet: %s", m_name, std::strerror(m_error));
		return false;
	}
	else
		return true;
}
