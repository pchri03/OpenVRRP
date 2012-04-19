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
#include "vrrp.h"

#include <arpa/inet.h>
#include <syslog.h>

int main ()
{
	openlog("openvrrp", LOG_PERROR, LOG_DAEMON);

	Addr addr;
	inet_pton(AF_INET, "192.168.1.3", &addr.ipv4);
	Vrrp vrrp("eth0", AF_INET, addr, 1);
	vrrp.addAddress("192.168.2.220");

	return MainLoop::run() ? 0 : -1;
}
