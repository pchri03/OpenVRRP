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
#include "telnetserver.h"
#include "configurator.h"

#include <iostream>
#include <cstdlib>

#include <getopt.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <syslog.h>

#define DEFAULT_CONFIG_FILE "configuration.dat"
#define DEFAULT_BIND_ADDR "127.0.0.1:7777"

static void cleanup ()
{
	VrrpManager::cleanup();
	VrrpSocket::cleanup();
}

static void showHelp ()
{
	std::cout <<
		"Usage: openvrrp [OPTIONS]\n"
		"VRRPv3 daemon\n"
		"\n"
		"  -c, --config=FILE  Set configuration data file to FILE (Default: " DEFAULT_CONFIG_FILE ")\n"
		"  -s, --stdout       Log to stdout instead of syslog\n"
		"  -b, --bind=ADDR    Bind to address / port (Default: " DEFAULT_BIND_ADDR ")\n"
		"  -h, --help         Display this message" << std::endl;			
}

int main (int argc, char *argv[])
{
	bool logToStdout = false;
	const char *configuration = DEFAULT_CONFIG_FILE;
	const char *bindAddr = DEFAULT_BIND_ADDR;
	for (;;)
	{
		static const option longOptions[] = {
			{"help", no_argument, 0, 'h'},
			{"config", required_argument, 0, 'c'},
			{"bind", required_argument, 0, 'b'},
			{"stdout", no_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		int optionIndex = 0;
		int c = getopt_long(argc, argv, "hc:b:s", longOptions, &optionIndex);
		if (c == -1)
			break;

		switch (c)
		{
			case 'c':
				configuration = optarg;
				break;

			case 'b':
				bindAddr = optarg;
				break;

			case 'h':
				showHelp();
				return 0;

			case 's':
				logToStdout = true;
				break;
		
			default:
				std::abort();
		}
	}

	if (logToStdout)
		openlog("openvrrp", LOG_PERROR, LOG_DAEMON);
	else
		openlog("openvrrp", 0, LOG_DAEMON);

	std::atexit(cleanup);

	TelnetServer server(bindAddr);
	if (!server.start())
		return -1;

	VrrpManager::removeVrrpInterfaces();

	Configurator::setConfigurationFile(configuration);
	Configurator::readConfiguration();

	return MainLoop::run() ? 0 : -1;
}
