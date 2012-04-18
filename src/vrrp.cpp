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

#include "vrrp.h"
#include "mainloop.h"

#include <cerrno>
#include <cstring>

#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <sys/types.h>

Vrrp::Vrrp (const char *interface, int family, std::uint8_t virtualRouterId, std::uint8_t priority) :
	m_virtualRouterId(virtualRouterId),
	m_priority(priority),
	m_advertisementInterval(1000),
	m_masterAdvertisementInterval(m_advertisementInterval),
	m_preemptMode(true),
	m_acceptMode(false),
	m_masterDownTimer(masterDownTimerCallback, this),
	m_advertisementTimer(advertisementTimerCallback, this),
	m_state(Initialize),
	m_family(family),
	m_socket(-1)
{
	std::strncpy(m_interface, interface, sizeof(m_interface));
	std::strncpy(m_outputInterface, interface, sizeof(m_outputInterface));

	initSocket();
	startup();
}

bool Vrrp::initSocket ()
{
	// Create VRRP socket
	m_socket = socket(m_family, SOCK_RAW, 112);
	if (m_socket == -1)
	{
		syslog(LOG_ERR, "Error creating VRRP socket: %m");
		return false;
	}

	// Bind to interface
	/*
	if (setsockopt(m_socket, SOL_SOCKET, SO_BINDTODEVICE, interface, std::strlen(interface)) == -1)
		syslog(LOG_WARNING, "Error binding to interface %s: %m", interface);
	*/

	// Set multicast TTL to 255
	int val = 255;
	if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val)) == -1)
		syslog(LOG_WARNING, "Error setting TTL: %m");

	return true;
}

bool Vrrp::joinMulticast (const char *interface)
{
	return modifyMulticast(interface, true);
}

bool Vrrp::leaveMulticast (const char *interface)
{
	return modifyMulticast(interface, false);
}

bool Vrrp::modifyMulticast (const char *interface, bool join)
{
	if (m_family == AF_INET)
	{
		ip_mreqn mreq;
		inet_pton(AF_INET, "224.0.0.18", &mreq.imr_multiaddr);
		mreq.imr_address.s_addr = 0;
		mreq.imr_ifindex = if_nametoindex(interface);
		if (setsockopt(m_socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
		{
			syslog(LOG_WARNING, "Error joining multicast on interface %s: %m", m_interface);
			return false;
		}
	}
	else // if (m_family == AF_INET6)
	{
		ipv6_mreq mreq;
		inet_pton(AF_INET6, "FF02::12", &mreq.ipv6mr_multiaddr);
		mreq.ipv6mr_interface = if_nametoindex(interface);
		if (setsockopt(m_socket, IPPROTO_IP, join ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
		{
			syslog(LOG_WARNING, "Error joining multicast on interface %s: %m", m_interface);
			return false;
		}
	}

	return true;
}

Vrrp::~Vrrp ()
{
	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		while (close(m_socket) == -1 && errno == EINTR);
	}
}

void Vrrp::advertisementTimerCallback (Timer *, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
		vrrp->onAdvertisementTimer();
}

void Vrrp::onAdvertisementTimer ()
{
	syslog(LOG_DEBUG, "Vrrp::onAdvertisementTimer()");
	if (m_state == Master)
	{
		// We're master and the advertisement timer fired, so send an advertisement
		sendAdvertisement();
		m_advertisementTimer.start(advertisementInterval());
	}
}

void Vrrp::startup ()
{
	if (m_priority == 255)
	{
		// We're starting up and we're the owner of the virtual IP addresses, so transition to master immediately
		sendAdvertisement();
		if (m_family == AF_INET)
			sendARPs();
		else // if (m_family == AF_INET6)
			sendNeighborAdvertisements();
		m_advertisementTimer.start(advertisementInterval());
		setState(Master);
	}
	else
	{
		// We're startup up, but we're not the owner. Transition to backup and wait for an advertisement from a master
		m_masterAdvertisementInterval = m_advertisementInterval;
		m_masterDownTimer.start(masterDownInterval());
		setState(Backup);
	}
}

void Vrrp::shutdown ()
{
	if (m_state == Backup)
	{
		// We're backup, so just stop our timer and switch to initialize state
		m_masterDownTimer.stop();
		setState(Initialize);
	}
	else if (m_state == Master)
	{
		// We're master, so inform everybody that we're leaving
		m_advertisementTimer.stop();
		sendAdvertisement(0);
		setState(Initialize);
	}
}

void Vrrp::masterDownTimerCallback (Timer *, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
		vrrp->onMasterDownTimer();
}

void Vrrp::onMasterDownTimer ()
{
	syslog(LOG_DEBUG, "Vrrp::onMasterDownTimer()");
	if (m_state == Backup)
	{
		// We're backup and the master down timer triggered, so we should transition to master
		setVirtualMac();
		sendAdvertisement();
		if (m_family == AF_INET)
			sendARPs();
		else // if (m_family == AF_INET6)
		{
			joinSolicitedNodeMulticasts();
			sendNeighborAdvertisements();
		}
		m_advertisementTimer.start(advertisementInterval());
		setState(Master);
	}
}

void Vrrp::socketCallback (int, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
		vrrp->onVrrpPacket();
}

void Vrrp::onVrrpPacket ()
{
	syslog(LOG_DEBUG, "Vrrp::onVrrpPacket()");

	SockAddr addr;
	socklen_t addrlen = sizeof(addr);
	ssize_t size;
	while ((size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, &addr.common, &addrlen)) == -1 && errno == EINTR);

	if (size > 0)
	{
		if (size < 8)
			return;

		// TODO - TTL

		// Verify version and type
		if (m_buffer[0] != 0x31)	// VRRPv3 ADVERTISEMENT
			return;

		// Verify virtual router id
		if (m_buffer[1] != virtualRouterId())
			return;

		std::uint8_t priority = m_buffer[2];
		std::uint8_t addrCount = m_buffer[3];
		std::uint16_t maxAdvertisementInterval = ((std::uint16_t)(m_buffer[4] & 0x0F) << 8) | m_buffer[5];

		// TODO - Checksum

		if (m_state == Backup)
		{
			if (priority == 0)
			{
				// The master decided to stop gracefully, wait skew time before transitioning to master
				m_masterDownTimer.start(skewTime());
			}
			else if (!preemptMode() || priority >= this->priority())
			{
				// The right master is running, wait for the next announcement
				setMasterAdvertisementInterval(maxAdvertisementInterval);
				m_masterDownTimer.start(masterDownInterval());

				// TODO Verify addrCount
			}
		}
		else if (m_state == Master)
		{
			if (priority == 0)
			{
				// The conflicting master is stopping gracefully, so just remind everybody that we are the master
				sendAdvertisement();
				m_advertisementTimer.start(advertisementInterval());
			}
			else if (priority > this->priority())	// TODO - Handle identical priorities
			{
				// The conflicting master has a higher priority than us, so we transition to backup
				m_advertisementTimer.stop();
				setMasterAdvertisementInterval(maxAdvertisementInterval);
				m_masterDownTimer.start(masterDownInterval());
				setState(Backup);
				setDefaultMac();
			}
			else
			{
				// The conflicting master has a lower priority than us. We expect it to transition to backup
			}
		}
	}
}

bool Vrrp::sendAdvertisement (int priority)
{
	if (priority == -1)
		priority = this->priority();

	m_buffer[0] = 0x31;	// VRRPv3 ADVERTISEMENT
	m_buffer[1] = virtualRouterId();
	m_buffer[2] = priority;
	m_buffer[3] = m_addrs.size();

	unsigned int advertisementInterval = this->advertisementInterval();
	m_buffer[4] = (advertisementInterval >> 8) & 0x0F;
	m_buffer[5] = advertisementInterval & 0xFF;

	// Checksum fields
	m_buffer[6] = 0;
	m_buffer[7] = 0;

	unsigned int addrSize = (m_family == AF_INET ? 4 : 16);
	unsigned int totalSize = 8 + m_addrs.size() * addrSize;

	if (sizeof(m_buffer) < totalSize)
	{
		syslog(LOG_ERR, "Too many addresses");
		return false;
	}

	std::uint8_t *ptr = m_buffer + 8;
	for (AddrList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
	{
		std::memcpy(ptr, &(*addr), addrSize);
		ptr += addrSize;
	}

	SockAddr addr;
	addr.common.sa_family = m_family;
	if (m_family == AF_INET)
	{
		addr.ipv4.sin_port = 0;
		inet_pton(AF_INET, "224.0.0.18", &addr.ipv4.sin_addr);
	}
	else // if (m_family == AF_INET6)
	{
		addr.ipv6.sin6_port = 0;
		addr.ipv6.sin6_flowinfo = 0; // TODO
		inet_pton(AF_INET6, "FF02::12", &addr.ipv6.sin6_addr);
		addr.ipv6.sin6_scope_id = if_nametoindex(m_outputInterface);
	}

	// TODO - Checksum

	if (sendto(m_socket, m_buffer, totalSize, 0, &addr.common, sizeof(addr)) == -1)
	{
		syslog(LOG_ERR, "Error sending VRRP advertisement: %m");
		return false;
	}

	return true;
}

bool Vrrp::sendARPs ()
{
	// Create packet socket
	int s = socket(AF_PACKET, SOCK_DGRAM, htons(ETHERTYPE_ARP));
	if (s == -1)
	{
		syslog(LOG_ERR, "Error creating ARP packet: %m");
		return false;
	}

	// Bind to interface
	struct sockaddr_ll addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = ETHERTYPE_ARP;
	addr.sll_ifindex = if_nametoindex(m_outputInterface);
	if (bind(s, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr)) == -1)
		syslog(LOG_ERR, "Error binding interface: %m");

	// Enable broadcast
	int val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) == -1)
		syslog(LOG_ERR, "Error enabling ARP broadcast: %m");

	// Prepare for broadcast destination
	addr.sll_halen = 6;
	static const std::uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	std::memcpy(addr.sll_addr, broadcastAddr, 6);

	// Fill the basic of the ARP packet
	struct
	{
		struct arphdr hdr;
		std::uint8_t body[20];
	} packet;

	packet.hdr.ar_hrd = htons(ARPHRD_ETHER);
	packet.hdr.ar_pro = htons(ETHERTYPE_IP);
	packet.hdr.ar_hln = 6;
	packet.hdr.ar_pln = 4;
	packet.hdr.ar_op = htons(ARPOP_REPLY);

	static const std::uint8_t mac[] = {0x00, 0x00, 0x5E, 0x00, 0x01, 0x00};
	std::memcpy(packet.body, mac, 5);
	packet.body[5] = virtualRouterId();
	std::memset(packet.body + 10, 0, 10);

	for (AddrList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
	{
		// Fill with the IPv4 address
		*reinterpret_cast<std::uint32_t *>(packet.body + 10) = addr->ipv4.s_addr;

		if (sendto(s, &packet, 28, 0, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
			syslog(LOG_ERR, "Error sending graticious ARP: %m");
	}

	while (close(s) == -1 && errno == EINTR);
}

bool Vrrp::sendNeighborAdvertisements ()
{
	// TODO
	return false;
}

bool Vrrp::joinSolicitedNodeMulticasts ()
{
	// TODO
	return false;
}

bool Vrrp::setVirtualMac ()
{
	// TODO
	return false;
}

bool Vrrp::setDefaultMac ()
{
	// TODO
	return false;
}

bool Vrrp::addMacvlanInterface ()
{
	// TODO
	return false;
}

bool Vrrp::removeMacvlanInterface ()
{
	// TODO
	return false;
}

bool Vrrp::addIpAddresses ()
{
	bool ret = true;
	for (AddrList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
		ret &= addIpAddress(*addr);
	return ret;
}

bool Vrrp::removeIpAddresses ()
{
	bool ret = true;
	for (AddrList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
		ret &= removeIpAddress(*addr);
	return ret;
}

bool Vrrp::addIpAddress (const Addr &addr)
{
	// TODO
	return false;
}

bool Vrrp::removeIpAddress (const Addr &addr)
{
	// TODO
	return false;
}


std::uint8_t Vrrp::virtualRouterId () const
{
	return m_virtualRouterId;
}

std::uint8_t Vrrp::priority () const
{
	return m_priority;
}

void Vrrp::setPriority (std::uint8_t priority)
{
	m_priority = priority;
}

unsigned int Vrrp::advertisementInterval () const
{
	return m_advertisementInterval;
}

void Vrrp::setAdvertisementInterval (unsigned int advertisementInterval)
{
	m_advertisementInterval = advertisementInterval;
}

unsigned int Vrrp::masterAdvertisementInterval () const
{
	return m_masterAdvertisementInterval;
}

void Vrrp::setMasterAdvertisementInterval (unsigned int masterAdvertisementInterval)
{
	m_masterAdvertisementInterval = masterAdvertisementInterval;
}

unsigned int Vrrp::skewTime () const
{
	return ((256 - priority()) * masterAdvertisementInterval()) / 256;
}

unsigned int Vrrp::masterDownInterval () const
{
	return 3 * masterAdvertisementInterval() + skewTime();
}

bool Vrrp::preemptMode () const
{
	return m_preemptMode;
}

void Vrrp::setPreemptMode (bool enabled)
{
	m_preemptMode = enabled;
}

bool Vrrp::acceptMode () const
{
	return m_acceptMode;
}

void Vrrp::setAcceptMode (bool enabled)
{
	m_acceptMode = enabled;
}

void Vrrp::setState (State state)
{
	m_state = state;
}

bool Vrrp::addAddress (const char *address)
{
	Addr addr;
	if (inet_pton(m_family, address, &addr) == 1)
	{
		m_addrs.push_back(addr);
		return true;
	}
	else
		return false;
}

bool Vrrp::removeAddress (const char *address)
{
	// TODO
	return false;
}
