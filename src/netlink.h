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

#ifndef INCLUDE_NETLINK_H
#define INCLUDE_NETLINK_H

#include "ipaddress.h"
#include "ipsubnet.h"

#include <cstdint>
#include <list>
#include <vector>
#include <map>
#include <string>

typedef std::map<int,std::string> InterfaceList;

struct nl_msg;
struct nl_sock;

class Netlink
{
	public:
		typedef void (InterfaceCallback)(int interface, bool linkIsUp, void *userData);

		static IpAddress getPrimaryIpAddress (int interface, int family);
		static bool addIpAddress (int interface, const IpSubnet &ip);
		static bool removeIpAddress (int interface, const IpSubnet &ip);

		static int addMacvlanInterface (int interface, const std::uint8_t *macAddress, const char *name);
		static int addVlanInterface(int interface, std::uint_fast16_t vlanId, const char* name);
		static bool removeInterface (int interface);
		static bool setMac (int interface, const std::uint8_t *macAddress);

		static bool toggleInterface (int interface, bool up);
		static bool isInterfaceUp (int interface);
		static InterfaceList interfaces ();

		static bool addInterfaceMonitor (int interface, InterfaceCallback *callback, void *userData);
		static bool removeInterfaceMonitor (int interface, InterfaceCallback *callback, void *userData);

	private:
		typedef std::pair<InterfaceCallback*, void *> CallbackData;
		typedef std::set<CallbackData> CallbackDataSet;
		typedef std::map<int,CallbackDataSet> CallbackMap;

		static bool modifyIpAddress (int interface, const IpSubnet &ip, bool add);
		static bool setIpConfiguration (const char *interface, const char *parameter, const char *value);
		static int addInterface(nl_msg* msg, const char* name);

		static void nlSocketCallback (int fd, void *userData);
		static int nlMessageCallback (nl_msg *msg, void *userData);
		static int nlSequenceCallback (nl_msg *msg, void *userData);

		static nl_sock *createSocket();

	private:
		static CallbackMap callbacks;
		static nl_sock *sock;
};

#endif // INCLUDE_NETLINK_H
