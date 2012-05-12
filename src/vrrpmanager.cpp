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
#include "vrrpmanager.h"
#include "vrrpservice.h"

VrrpManager::VrrpServiceMap VrrpManager::m_services;

VrrpService *VrrpManager::getService (int interface, std::uint_fast8_t virtualRouterId, int family, bool createIfMissing)
{
	VrrpServiceMap::const_iterator interfaceServices = m_services.find(interface);
	if (interfaceServices != m_services.end())
	{
		VrrpServiceMap::mapped_type::const_iterator routerServices = interfaceServices->second.find(virtualRouterId);
		if (routerServices != interfaceServices->second.end())
		{
			VrrpServiceMap::mapped_type::mapped_type::const_iterator service = routerServices->second.find(family);
			if (service != routerServices->second.end())
				return service->second;
		}
	}

	if (createIfMissing)
	{
		VrrpService *service = new VrrpService(interface, family, virtualRouterId);
		if (service->error() == 0)
		{
			m_services[interface][virtualRouterId][family] = service;
			return service;
		}
		else
		{
			delete service;
			return 0;
		}
	}
	else
		return 0;
}

void VrrpManager::cleanup ()
{
	for (VrrpServiceMap::const_iterator interfaceServices = m_services.begin(); interfaceServices != m_services.end(); ++interfaceServices)
	{
		for (VrrpServiceMap::mapped_type::const_iterator routerServices = interfaceServices->second.begin(); routerServices != interfaceServices->second.end(); ++routerServices)
		{
			for (VrrpServiceMap::mapped_type::mapped_type::const_iterator service = routerServices->second.begin(); service != routerServices->second.end(); ++service)
				delete service->second;
		}
	}

}
