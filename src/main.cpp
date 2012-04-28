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

#include "mainloop.h"
#include "vrrpservice.h"
#include "vrrpmanager.h"
#include "vrrpsocket.h"
#include "ipaddress.h"

#include <net/if.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <iostream>
#include <cstdlib>

static void cleanup ()
{
	VrrpManager::cleanup();
	VrrpSocket::cleanup();
}

int main ()
{
	openlog("openvrrp", LOG_PERROR, LOG_DAEMON);

	std::atexit(cleanup);

	int interface = if_nametoindex("eth0");
	VrrpService *service;

	service = VrrpManager::getService(interface, 1, AF_INET, true);
	if (service != nullptr)
	{
		service->addIpAddress("192.168.2.220");
		service->startup();
	}

	service = VrrpManager::getService(interface, 1, AF_INET6, true);
	if (service != nullptr)
	{
		service->addIpAddress("FEF0::3");
		service->startup();
	}

	return MainLoop::run() ? 0 : -1;
}
