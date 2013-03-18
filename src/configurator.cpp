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

#include "configurator.h"
#include "vrrpmanager.h"
#include "vrrpservice.h"

#include <cstdint>
#include <cstring>
#include <fstream>

#include <net/if.h>

/*
   Configuration format is as follows:

   config {
	int version;
	int routerCount;
	router[routerCount] router;
   }
   router {
    string interface;
	int vrid;
	int addressFamily;
	int priority;
	int interval;
	bool accept;
	bool preempt;
	bool enabled;
	int flags;
    IpAddress primaryIp; // Only if bit 0 of flags is set
	int addressCount;
	IpSubnet[addressCount] subnets;
   }
*/	

const char *Configurator::filename = 0;

bool Configurator::readConfiguration (const char *filename)
{
	std::ifstream file(filename == 0 ? Configurator::filename : filename);
	if (!file.good())
		return false;

	int version;
	if (!readInt(file, version) || version != 1)
		return false;

	int routerCount;
	if (!readInt(file, routerCount) || routerCount < 0)
		return false;
	
	for (int i = 0; i != routerCount; ++i)
	{
		std::string interface;
		int vrid;
		int addressFamily;
		int priority;
		int interval;
		bool accept;
		bool preempt;
		bool enabled;
		int flags;
		IpAddress primaryIp;
		int addressCount;
		IpSubnetSet subnets;

		// Read data
		if (
				!readString(file, interface)
				|| !readInt(file, vrid)
				|| !readInt(file, addressFamily)
				|| !readInt(file, priority)
				|| !readInt(file, interval)
				|| !readBoolean(file, accept)
				|| !readBoolean(file, preempt)
				|| !readBoolean(file, enabled)
				|| !readInt(file, flags))
		{
			return false;
		}

		if (flags & 1)
		{
			if (!readIp(file, primaryIp))
				return false;
		}

		if (!readInt(file, addressCount))
			return false;

		for (int j = 0; j != addressCount; ++j)
		{
			IpSubnet subnet;
			if (!readSubnet(file, subnet))
				return false;

			subnets.insert(subnet);
		}

		// Sanitize

		if (vrid < 1 || vrid > 255)
		{
			// TODO - Add to log
			continue;
		}

		if (priority < 1 || priority > 255)
		{
			// TODO - Add to log
			continue;
		}

		if (addressFamily != AF_INET && addressFamily != AF_INET6)
		{
			// TODO - Add to log
			continue;
		}

		if (interval % 10 != 0 || interval < 10 || interval > 40950)
		{
			// TODO - Add to log
			continue;
		}

		int ifIndex = if_nametoindex(interface.c_str());
		if (ifIndex <= 0)
		{
			// TODO - Add to log
			continue;
		}

		// Create service

		VrrpService *service = VrrpManager::getService(ifIndex, vrid, addressFamily, true);
		if (service == 0)
		{
			// TODO - Add to log
			continue;
		}

		service->setPriority(priority);
		service->setAdvertisementInterval(interval / 10);
		service->setAcceptMode(accept);
		service->setPreemptMode(preempt);
		if (flags & 1)
			service->setPrimaryIpAddress(primaryIp);
		for (IpSubnetSet::const_iterator subnet = subnets.begin(); subnet != subnets.end(); ++subnet)
		{
			service->addIpAddress(*subnet);
		}
		if (enabled)
			service->enable();
		else
			service->disable();
	}

	return true;
}

std::vector<VrrpService *> Configurator::services ()
{
	std::vector<VrrpService *> services;

	const VrrpManager::VrrpServiceMap& interfaces = VrrpManager::services();
	for (VrrpManager::VrrpServiceMap::const_iterator interface = interfaces.begin(); interface != interfaces.end(); ++interface)
	{
		for (VrrpManager::VrrpServiceMap::mapped_type::const_iterator routers = interface->second.begin(); routers != interface->second.end(); ++routers)
		{
			for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = routers->second.begin(); service != routers->second.end(); ++service)
			{
				services.push_back(service->second);
			}
		}
	}

	return services;
}

bool Configurator::writeConfiguration (const char *filename)
{
	std::ofstream file(filename == 0 ? Configurator::filename : filename, std::ios_base::out | std::ios_base::trunc);
	if (!file.good())
		return false;

	if (!writeInt(file, 1))
		return false;

	std::vector<VrrpService *> services = Configurator::services();
	if (!writeInt(file, services.size()))
		return false;

	for (std::vector<VrrpService *>::const_iterator it = services.begin(); it != services.end(); ++it)
	{
		const VrrpService *service = *it;

		char ifname[IFNAMSIZ];
		if_indextoname(service->interface(), ifname);

		if (
				!writeString(file, ifname)
				|| !writeInt(file, service->virtualRouterId())
				|| !writeInt(file, service->family())
				|| !writeInt(file, service->priority())
				|| !writeInt(file, service->advertisementInterval() * 10)
				|| !writeBoolean(file, service->acceptMode())
				|| !writeBoolean(file, service->preemptMode())
				|| !writeBoolean(file, service->enabled()))
			return false;

		if (!writeInt(file, 0)) // TODO - Support storing primary ip
			return false;

		const IpSubnetSet &subnets = service->subnets();
		if (!writeInt(file, subnets.size()))
			return false;

		for (IpSubnetSet::const_iterator subnet = subnets.begin(); subnet != subnets.end(); ++subnet)
		{
			if (!writeSubnet(file, *subnet))
				return false;
		}
	}

	return true;
}

bool Configurator::readInt (std::istream &stream, int &value)
{
	std::uint32_t buffer;
	if (stream.readsome(reinterpret_cast<char *>(&buffer), sizeof(buffer)) != sizeof(buffer) || !stream.good())
		return false;
	value = (int)ntohl(buffer);
	return true;
}

bool Configurator::writeInt (std::ostream &stream, int value)
{
	std::uint32_t buffer = htonl(value);
	return stream.write(reinterpret_cast<const char *>(&buffer), sizeof(buffer)).good();
}

bool Configurator::readBoolean (std::istream &stream, bool &value)
{
	std::uint8_t buffer;
	if (stream.readsome(reinterpret_cast<char *>(&buffer), sizeof(buffer)) != sizeof(buffer) || !stream.good())
		return false;
	value = (buffer != 0);
	return true;
}

bool Configurator::writeBoolean (std::ostream &stream, bool value)
{
	std::uint8_t buffer = (value ? 0xFF : 0x00);
	return stream.write(reinterpret_cast<const char *>(&buffer), sizeof(buffer)).good();
}

bool Configurator::readString (std::istream &stream, std::string &value)
{
	std::uint8_t size;
	char buffer[255];
	if (stream.readsome(reinterpret_cast<char *>(&size), sizeof(size)) != sizeof(size) || !stream.good())
		return false;

	if (stream.readsome(buffer, size) != size || !stream.good())
		return false;

	value.assign(buffer, size);
	return true;
}

bool Configurator::writeString (std::ostream &stream, const std::string &value)
{
	std::uint8_t buffer[256];
	std::string::size_type size = value.size();
	if (size > 255)
		size = 255;

	buffer[0] = size;
	std::memcpy(buffer + 1, value.c_str(), size);

	return stream.write(reinterpret_cast<const char *>(buffer), size + 1).good();
}

bool Configurator::readIp (std::istream &stream, IpAddress &address)
{
	int family;
	if (!readInt(stream, family))
		return false;

	unsigned int size = IpAddress::familySize(family);
	if (size > 0)
	{
		std::uint8_t *buffer = new std::uint8_t[size];
		if (stream.readsome(reinterpret_cast<char *>(buffer), size) != size || !stream.good())
		{
			delete[] buffer;
			return false;
		}

		address = IpAddress(buffer, family);
		delete[] buffer;
	}
	else
		address = IpAddress();

	return true;
}

bool Configurator::writeIp (std::ostream &stream, const IpAddress &address)
{
	if (!writeInt(stream, address.family()))
		return false;

	if (!stream.write(reinterpret_cast<const char *>(address.data()), address.size()) || !stream.good())
		return false;

	return true;
}

bool Configurator::readSubnet (std::istream &stream, IpSubnet &subnet)
{
	IpAddress address;
	int cidr;
	if (!readIp(stream, address) || !readInt(stream, cidr))
		return false;

	subnet = IpSubnet(address, cidr);
	return true;
}

bool Configurator::writeSubnet (std::ostream &stream, const IpSubnet &subnet)
{
	return writeIp(stream, subnet.address()) && writeInt(stream, subnet.cidr());
}

void Configurator::setConfigurationFile (const char *filename)
{
	Configurator::filename = filename;
}
