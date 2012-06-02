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

#ifndef INCLUDE_CONFIGURATION_H
#define INCLUDE_CONFIGURATION_H

#include <list>
#include <vector>
#include <cstdint>

#include "ipaddress.h"

class Configuration;

typedef std::list<Configuration> ConfigurationList;

class Configuration
{
	public:
		Configuration (int family, const char *name);
		~Configuration ()
		{
		}

		const char *name () const
		{
			return m_name.data();
		}

		int family () const
		{
			return m_family;
		}

		int interface () const
		{
			return m_interface;
		}

		void setInterface (int interface)
		{
			m_interface = interface;
		}

		std::uint_fast8_t virtualRouterId () const
		{
			return m_virtualRouterId;
		}

		void setVirtualRouterId (std::uint_fast8_t virtualRouterId)
		{
			m_virtualRouterId = virtualRouterId;
		}

		std::uint_fast8_t priority () const
		{
			return m_priority;
		}

		void setPriority (std::uint_fast8_t priority)
		{
			m_priority = priority;
		}

		bool preemptionMode () const
		{
			return m_preemptionMode;
		}

		void setPreemptionMode (bool preemptionMode)
		{
			m_preemptionMode = preemptionMode;
		}

		bool acceptMode () const
		{
			return m_acceptMode;
		}

		void setAcceptMode (bool acceptMode)
		{
			m_acceptMode = acceptMode;
		}

		unsigned int advertisementInterval () const
		{
			return m_advertisementInterval;
		}

		void setAdvertisementInterval (unsigned int advertisementInterval)
		{
			m_advertisementInterval = advertisementInterval;
		}

		IpAddress primaryIpAddress () const
		{
			return m_primaryIpAddress;
		}

		void setPrimaryIpAddress (const IpAddress &primaryIpAddress)
		{
			m_primaryIpAddress = primaryIpAddress;
		}

		IpAddressList addresses () const
		{
			return m_addresses;
		}

		void addAddress (const IpAddress &address)
		{
			m_addresses.push_back(address);
		}

		static ConfigurationList parseConfigurationFile (const char *filename);

	private:
		int m_family;
		std::vector<char> m_name;
		int m_interface;
		std::uint_fast8_t m_virtualRouterId;
		std::uint_fast8_t m_priority;
		IpAddress m_primaryIpAddress;
		unsigned int m_advertisementInterval;
		bool m_preemptionMode;
		bool m_acceptMode;
		IpAddressList m_addresses;
};

#endif // INCLUDE_CONFIGURATION_H
