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

#include "configuration.h"

#include <cstring>
#include <fstream>

#include <net/if.h>

Configuration::Configuration (int family, const char *name) :
	m_family(family),
	m_virtualRouterId(0),
	m_priority(100),
	m_advertisementInterval(100),
	m_preemptionMode(true),
	m_acceptMode(true)
{
	int size = std::strlen(name);
	m_name.resize(size);
	std::memcpy(m_name.data(), name, size);
}

ConfigurationList Configuration::parseConfigurationFile (const char *filename)
{
	ConfigurationList list;

	Configuration config(AF_INET, "Test");
	config.setInterface(if_nametoindex("eth0"));
	config.setVirtualRouterId(1);
	config.addAddress("192.168.1.10");
	list.push_back(config);

	return list;
}
