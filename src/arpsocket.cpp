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

#include "arpsocket.h"

#include <cerrno>
#include <cstring>

#include <syslog.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <sys/socket.h>

bool ArpSocket::sendGratuitiousArp (unsigned int interface, const IpAddress &address)
{
	if (address.family() != AF_INET)
		return false;

	int s = socket(AF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, htons(ETH_P_ARP));
	if (s == -1)
	{
		syslog(LOG_ERR, "Error creating ARP socket: %s", std::strerror(errno));
		return false;
	}

	// Bind to specific interface
	sockaddr_ll addr;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ARP);
	addr.sll_ifindex = interface;
	if (bind(s, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		syslog(LOG_ERR, "Error binding ARP socket to interface: %s", std::strerror(errno));
		close(s);
		return false;
	}

	// Disable packet reception
	shutdown(s, SHUT_RD);

	// Enable broadcast
	int val = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val));

	// Get MAC
	ifreq req;
	if_indextoname(interface, req.ifr_name);
	if (ioctl(s, SIOCGIFHWADDR, &req) == -1)
	{
		syslog(LOG_ERR, "Error getting hardware address from interface: %s", std::strerror(errno));
		close(s);
		return false;
	}

	// Prepare ARP packet

	struct ArpPacket
	{
		std::uint16_t hardwareType;
		std::uint16_t protocolType;
		std::uint8_t hardwareAddressLength;
		std::uint8_t protocolAddressLength;
		std::uint16_t operation;
		std::uint8_t senderHardwareAddress[6];
		std::uint8_t senderProtocolAddress[4];
		std::uint8_t targetHardwareAddress[6];
		std::uint8_t targetProtocolAddress[4];
	} packet;

	packet.hardwareType = htons(ARPHRD_ETHER);
	packet.protocolType = htons(ETHERTYPE_IP);
	packet.hardwareAddressLength = 6;
	packet.protocolAddressLength = 4;
	packet.operation = htons(ARPOP_REPLY);
	std::memcpy(&packet.senderHardwareAddress, req.ifr_hwaddr.sa_data, 6);
	std::memcpy(&packet.senderProtocolAddress, address.data(), 4);
	std::memset(&packet.targetHardwareAddress, 0, 6);
	std::memcpy(&packet.targetProtocolAddress, address.data(), 4);

	// Broadcast packet
	std::memset(&addr.sll_addr, 0xFF, 6);
	addr.sll_halen = 6;

	bool ret;
	if (sendto(s, &packet, sizeof(packet), 0, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		syslog(LOG_ERR, "Error sending ARP packet: %s", std::strerror(errno));
		ret = false;
	}
	else
		ret = true;

	return ret;
}
