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

#include "arpservice.h"
#include "mainloop.h"

#include <cstring>

#include <unistd.h>
#include <syslog.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <sys/socket.h>

ArpService::ServiceMap ArpService::services;

void ArpService::addFakeArp (int interface, const IpAddress &address, const std::uint8_t *mac)
{
	ServiceMap::const_iterator it = services.find(interface);
	ArpService *service;
	if (it == services.end())
	{
		service = new ArpService(interface);
		if (service->m_socket == -1)
		{
			delete service;
			return;
		}
		services[interface] = service;
	}
	else
		service = it->second;

	std::uint8_t *tmp = new std::uint8_t[6];
	std::memcpy(tmp, mac, 6);
	service->m_addresses[address] = tmp;
}

void ArpService::removeFakeArp (int interface, const IpAddress &address)
{
	ServiceMap::iterator it = services.find(interface);
	if (it == services.end())
		return;

	ArpService *service = it->second;

	IpAddressMap::iterator addressIt = service->m_addresses.find(address);
	if (addressIt != service->m_addresses.end())
	{
		delete addressIt->second;
		service->m_addresses.erase(addressIt);
		if (service->m_addresses.size() == 0)
		{
			delete service;
			services.erase(it);
		}
	}
}

ArpService::ArpService (int interface)
	: m_socket(socket(AF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, htons(ETH_P_ARP)))
{
	if (m_socket == -1)
	{
		syslog(LOG_ERR, "Error creating ARP socket: %s", std::strerror(errno));
		return;
	}

	// Bind to interface
	sockaddr_ll addr;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ARP);
	addr.sll_ifindex = interface;
	if (bind(m_socket, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
	{
		syslog(LOG_ERR, "Error binding ARP socket to interface: %s", std::strerror(errno));
		close(m_socket);
		m_socket = -1;
		return;
	}

	if (!MainLoop::addMonitor(m_socket, socketCallback, this))
	{
		close(m_socket);
		m_socket = -1;
	}
}

ArpService::~ArpService ()
{
	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		close(m_socket);
	}
}

void ArpService::socketCallback (int, void *userData)
{
	ArpService *service = reinterpret_cast<ArpService *>(userData);
	service->onArpPacket();	
}

void ArpService::onArpPacket ()
{
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
	sockaddr_ll addr;
	socklen_t addrlen = sizeof(addr);
	int size;

	// Receive packet
	do
	{
		size = recvfrom(m_socket, reinterpret_cast<void *>(&packet), sizeof(packet), 0, reinterpret_cast<sockaddr *>(&addr), &addrlen);
	} while (size == -1 && errno == EINTR);

	if (size == -1)
	{
		syslog(LOG_ERR, "Error receiving ARP packet: %s", std::strerror(errno));
		return;
	}

	// Verify fields
	if (size != sizeof(packet) || packet.hardwareType != htons(ARPHRD_ETHER) || packet.protocolType != htons(ETHERTYPE_IP) || packet.hardwareAddressLength != 6 || packet.protocolAddressLength != 4 || packet.operation != htons(ARPOP_REQUEST))
		return;

	// Look for MAC
	IpAddress address(packet.targetProtocolAddress, AF_INET);
	IpAddressMap::const_iterator it = m_addresses.find(address);
	if (it == m_addresses.end())
		return;

	// Create response
	packet.operation = htons(ARPOP_REPLY);
	std::memcpy(packet.targetHardwareAddress, packet.senderHardwareAddress, sizeof(packet.senderHardwareAddress));
	std::memcpy(packet.targetProtocolAddress, packet.senderProtocolAddress, sizeof(packet.senderProtocolAddress));
	
	std::memcpy(packet.senderHardwareAddress, it->second, sizeof(packet.senderHardwareAddress));
	std::memcpy(packet.senderProtocolAddress, address.data(), sizeof(packet.senderProtocolAddress));


	do
	{
		size = sendto(m_socket, reinterpret_cast<const void *>(&packet), sizeof(packet), 0, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
	} while (size == -1 && errno == EINTR);

	if (size == -1)
		syslog(LOG_ERR, "Error sending ARP packet: %s", std::strerror(errno));
}
