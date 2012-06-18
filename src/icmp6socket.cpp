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

#include "icmp6socket.h"
#include "ipaddress.h"
#include "util.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <sys/socket.h>

bool Icmp6Socket::sendNeighborAdvertisement (unsigned int interface, const IpAddress &address)
{
	if (address.family() != AF_INET6)
		return false;

	int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
	if (s == -1)
	{
		syslog(LOG_ERR, "Error creating ICMPv6 socket: %s", std::strerror(errno));
		return -1;
	}

	if (bind(s, address.socketAddress(), address.socketAddressSize()) == -1)
		syslog(LOG_WARNING, "Error binding ICMPv6 socket to IP: %s", std::strerror(errno));

	char ifname[IFNAMSIZ];
	if_indextoname(interface, ifname);
	if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, ifname, std::strlen(ifname)) == -1)
		syslog(LOG_WARNING, "Error binding ICMPv6 socket to interface: %s", std::strerror(errno));

	int ttl = 225;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl)) == -1)
		syslog(LOG_WARNING, "Error setting hop limit of ICMPv6 socket: %s", std::strerror(errno));

	nd_neighbor_advert packet;
	packet.nd_na_hdr.icmp6_type = ND_NEIGHBOR_ADVERT;
	packet.nd_na_hdr.icmp6_code = 0;
	packet.nd_na_hdr.icmp6_cksum = 0;
	packet.nd_na_hdr.icmp6_data32[0] = htonl(0xA0000000); // R and O flags se
	std::memcpy(&packet.nd_na_target, address.data(), address.size());

	IpAddress allNodesMulticastAddress("ff02::1");

	packet.nd_na_hdr.icmp6_cksum = Util::checksum(&packet, sizeof(packet), address, allNodesMulticastAddress, 58);

	bool ret;
	if (sendto(s, &packet, sizeof(packet), 0, allNodesMulticastAddress.socketAddress(), allNodesMulticastAddress.socketAddressSize()) == -1)
	{
		syslog(LOG_ERR, "Error sending neighbor advertisement: %s", std::strerror(errno));
		ret = false;
	}
	else
		ret = true;

	close(s);

	return ret;
}
