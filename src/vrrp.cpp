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
#include "netlink.h"

#include <cerrno>
#include <cstring>

#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

Vrrp::Vrrp (const char *interface, int family, const IpAddress &primaryAddr, std::uint8_t virtualRouterId, std::uint8_t priority) :
	m_virtualRouterId(virtualRouterId),
	m_priority(priority),
	m_primaryIpAddress(primaryAddr),
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
	static const std::uint8_t mac[] = {0x00, 0x00, 0x5E, 0x00};
	std::memcpy(m_mac, mac, 4);
	m_mac[4] = (family == AF_INET ? 1 : 2);
	m_mac[5] = virtualRouterId;

	if (family == AF_INET)
		m_multicastAddress = IpAddress("224.0.0.18");
	else // if (family == AF_INET6)
		m_multicastAddress = IpAddress("FF02::12");

	m_interface = if_nametoindex(interface);
	m_outputInterface = Netlink::addMacvlanInterface(m_interface, m_mac);
	if (m_outputInterface < 0)
		m_outputInterface = m_interface;

	initSocket();
	startup();
}

Vrrp::~Vrrp ()
{
	shutdown();

	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		while (close(m_socket) == -1 && errno == EINTR);
	}

	if (m_outputInterface != m_interface)
		Netlink::removeInterface(m_outputInterface);
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
	if (bind(m_socket, m_primaryIpAddress.socketAddress(), m_primaryIpAddress.socketAddressSize()) == -1)
	{
		syslog(LOG_ERR, "Error binding to address: %m");
		return false;
	}

	// Set multicast TTL to 255
	int val = 255;
	if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val)) == -1)
		syslog(LOG_WARNING, "Error setting TTL: %m");

	MainLoop::addMonitor(m_socket, socketCallback, this);

	return true;
}

bool Vrrp::joinMulticast (int interface)
{
	return modifyMulticast(interface, true);
}

bool Vrrp::leaveMulticast (int interface)
{
	return modifyMulticast(interface, false);
}

bool Vrrp::modifyMulticast (int interface, bool join)
{
	if (m_family == AF_INET)
	{
		ip_mreqn mreq;
		inet_pton(AF_INET, "224.0.0.18", &mreq.imr_multiaddr);
		mreq.imr_address.s_addr = 0;
		mreq.imr_ifindex = interface;
		if (setsockopt(m_socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
		{
			syslog(LOG_WARNING, "Error joining multicast: %m");
			return false;
		}
	}
	else // if (m_family == AF_INET6)
	{
		ipv6_mreq mreq;
		inet_pton(AF_INET6, "FF02::12", &mreq.ipv6mr_multiaddr);
		mreq.ipv6mr_interface = interface;
		if (setsockopt(m_socket, IPPROTO_IP, join ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
		{
			syslog(LOG_WARNING, "Error joining multicast: %m");
			return false;
		}
	}

	return true;
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
		setState(Master);
		if (m_family == AF_INET)
			sendARPs();
		else // if (m_family == AF_INET6)
			sendNeighborAdvertisements();
		m_advertisementTimer.start(advertisementInterval());
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
		setState(Master);
		if (m_family == AF_INET)
			sendARPs();
		else // if (m_family == AF_INET6)
		{
			joinSolicitedNodeMulticasts();
			sendNeighborAdvertisements();
		}
		m_advertisementTimer.start(advertisementInterval());
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

	IpAddress addr;
	socklen_t addrlen = addr.size();
	ssize_t size;
	while ((size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, addr.socketAddress(), &addrlen)) == -1 && errno == EINTR);

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

		if (checksum(m_buffer, size, addr, m_primaryIpAddress) != 0)
			return;

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

	unsigned int advertisementInterval = this->advertisementInterval() / 10;
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
	for (IpAddressList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
	{
		std::memcpy(ptr, addr->data(), addr->size());
		ptr += addr->size();
	}

	*reinterpret_cast<std::uint16_t *>(m_buffer + 6) = checksum(m_buffer, totalSize, m_primaryIpAddress, m_multicastAddress);

	if (sendto(m_socket, m_buffer, totalSize, 0, m_multicastAddress.socketAddress(), m_multicastAddress.socketAddressSize()) == -1)
	{
		syslog(LOG_ERR, "Error sending VRRP advertisement: %m");
		return false;
	}

	return true;
}

bool Vrrp::sendARPs ()
{
	// Create packet socket
	int s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	if (s == -1)
	{
		syslog(LOG_ERR, "Error creating ARP packet: %m");
		return false;
	}

	// Bind to interface
	struct sockaddr_ll addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ARP);
	addr.sll_ifindex = m_outputInterface;
	if (bind(s, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr)) == -1)
		syslog(LOG_ERR, "Error binding to interface: %m");

	// Enable broadcast
	int val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) == -1)
		syslog(LOG_ERR, "Error enabling ARP broadcast: %m");

	// Fill the basic of the ARP packet
	std::uint8_t buffer[42];
	std::memset(buffer, 0xFF, 6);
	std::memcpy(buffer + 6, m_mac, 6);
	*reinterpret_cast<std::uint16_t *>(buffer + 12) = htons(ETH_P_ARP);
	
	*reinterpret_cast<std::uint16_t *>(buffer + 14) = htons(ARPHRD_ETHER);
	*reinterpret_cast<std::uint16_t *>(buffer + 16) = htons(ETH_P_IP);
	buffer[18] = 6;
	buffer[19] = 4;
	*reinterpret_cast<std::uint16_t *>(buffer + 20) = htons(ARPOP_REPLY);
	std::memcpy(buffer + 22, m_mac, 6);
	std::memset(buffer + 32, 0, 6);

	for (IpAddressList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
	{
		// Fill with the IPv4 address
		*reinterpret_cast<std::uint32_t *>(buffer + 28) = *reinterpret_cast<const std::uint32_t *>(addr->data());
		*reinterpret_cast<std::uint32_t *>(buffer + 38) = *reinterpret_cast<const std::uint32_t *>(addr->data());;

		if (write(s, buffer, 42) == -1)
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
	if (m_outputInterface == m_interface)
		return Netlink::setMac(m_outputInterface, m_mac);
	else
		return Netlink::toggleInterface(m_outputInterface, true);
}

bool Vrrp::setDefaultMac ()
{
	if (m_outputInterface == m_interface)
	{
		struct
		{
			std::uint32_t cmd;
			std::uint32_t size;
			std::uint8_t mac[6];
		} packet;
		packet.cmd = ETHTOOL_GPERMADDR;
		packet.size = 6;
		
		struct ifreq req;
		if_indextoname(m_interface, req.ifr_ifrn.ifrn_name);
		req.ifr_ifru.ifru_data = reinterpret_cast<__caddr_t>(&packet);

		if (ioctl(m_socket, SIOCETHTOOL, &req) == -1)
		{
			syslog(LOG_ERR, "Error getting permanent hardware address: %m");
			return false;
		}

		return Netlink::setMac(m_outputInterface, packet.mac);
	}
	else
		return Netlink::toggleInterface(m_outputInterface, false);
}

bool Vrrp::addIpAddresses ()
{
	bool ret = true;
	for (IpAddressList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
		ret &= (Netlink::addIpAddress(m_outputInterface, *addr));
	return ret;
}

bool Vrrp::removeIpAddresses ()
{
	bool ret = true;
	for (IpAddressList::const_iterator addr = m_addrs.begin(); addr != m_addrs.end(); ++addr)
		ret &= (Netlink::removeIpAddress(m_outputInterface, *addr));
	return ret;
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
	if (state == Master)
	{
		if (m_outputInterface != m_interface)
			Netlink::toggleInterface(m_outputInterface, true);
		setVirtualMac();
		addIpAddresses();
		sendAdvertisement();
	}
	else
	{
		removeIpAddresses();
		setDefaultMac();
	}
}

bool Vrrp::addIpAddress (const IpAddress &address)
{
	if (address.family() != m_family)
		return false;
	m_addrs.push_back(address);
	return true;
}

bool Vrrp::removeIpAddress (const IpAddress &address)
{
	// TODO
	return false;
}

std::uint16_t Vrrp::checksum (const void *packet, unsigned int size, const IpAddress &srcAddr, const IpAddress &dstAddr) const
{
	std::uint8_t pseudoHeader[40];
	unsigned int pseudoHeaderSize;
	if (m_family == AF_INET)
	{
		std::memcpy(pseudoHeader, srcAddr.data(), 4);
		std::memcpy(pseudoHeader + 4, dstAddr.data(), 4);
		pseudoHeader[8] = 0;
		pseudoHeader[9] = 112;
		*reinterpret_cast<std::uint16_t *>(pseudoHeader + 10) = htons(size);
		pseudoHeaderSize = 12;
	}
	else // if (m_family == AF_INET6)
	{
		std::memcpy(pseudoHeader, srcAddr.data(), 16);
		std::memcpy(pseudoHeader + 16, dstAddr.data(), 16);
		*reinterpret_cast<std::uint32_t *>(pseudoHeader + 32) = htonl(size);
		*reinterpret_cast<std::uint32_t *>(pseudoHeader + 36) = htonl(112);
		pseudoHeaderSize = 40;
	}

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
