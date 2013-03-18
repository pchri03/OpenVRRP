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

#ifndef INCLUDE_CONFIGURATOR_H
#define INCLUDE_CONDIFURATOR_H

#include <string>
#include <vector>

#include "ipaddress.h"
#include "ipsubnet.h"

class VrrpService;

class Configurator
{
	public:
		static void setConfigurationFile (const char *filename);
		static bool readConfiguration (const char *filename = 0);
		static bool writeConfiguration (const char *filename = 0);

	private:
		static bool readInt (std::istream &stream, int &value);
		static bool readString (std::istream &stream, std::string &value);
		static bool readBoolean (std::istream &stream, bool &value);
		static bool readIp (std::istream &stream, IpAddress &address);
		static bool readSubnet (std::istream &stream, IpSubnet &subnet);
		
		static bool writeInt (std::ostream &stream, int value);
		static bool writeString (std::ostream &stream, const std::string &value);
		static bool writeBoolean (std::ostream &stream, bool value);
		static bool writeIp (std::ostream &stream, const IpAddress &address);
		static bool writeSubnet (std::ostream &stream, const IpSubnet &subnet);

		static std::vector<VrrpService *> services ();

	private:
		static const char *filename;
};

#endif // INCLUDE_CONFIGURATOR_H
