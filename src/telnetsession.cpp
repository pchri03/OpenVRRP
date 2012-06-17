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

#include "telnetsession.h"
#include "telnetserver.h"
#include "mainloop.h"
#include "vrrpsocket.h"
#include "vrrpmanager.h"
#include "vrrpservice.h"

#include <cstring>
#include <cstdarg>
#include <cstdio>

#include <unistd.h>
#include <syslog.h>
#include <net/if.h>

#define RESP_WELCOME			"OpenVRRP CLI\n"

#define RESP_INVALID_COMMAND	"Invalid command\n"
#define RESP_NO_SUCH_ROUTER		"No such router\n"

#define RESP_SHOW_ROUTER_STAT_INTERFACE	"show router stat interface INTF [VRID]\n"

#define RESP_SHOW_ROUTER_STAT	"show router stat [VRID]\n" \
								RESP_SHOW_ROUTER_STAT_INTERFACE

#define RESP_SHOW_ROUTER_INTERFACE	"show router interface INTF [VRID]\n" \

#define RESP_SHOW_ROUTER		"show router [VRID]\n" \
								RESP_SHOW_ROUTER_INTERFACE \
								RESP_SHOW_ROUTER_STAT

#define RESP_SHOW				RESP_SHOW_ROUTER \
								"show stat\n"

#define RESP_HELP				"exit\n" \
								"help\n" \
								RESP_SHOW

#define SEND_RESP(str)	write(m_socket, str, sizeof(str) -1)

TelnetSession::TelnetSession (int fd, TelnetServer *server) :
	m_socket(fd),
	m_bufferSize(0),
	m_server(server),
	m_overflow(false)
{
	MainLoop::addMonitor(m_socket, onIncomingData, this);
	SEND_RESP(RESP_WELCOME);
}

TelnetSession::~TelnetSession ()
{
	MainLoop::removeMonitor(m_socket);
	while (close(m_socket) == -1 && errno == EINTR);
	m_server->removeSession(this);
}

void TelnetSession::onIncomingData (int, void *userData)
{
	TelnetSession *self = reinterpret_cast<TelnetSession *>(userData);
	self->receiveChunk();
}

void TelnetSession::receiveChunk ()
{
	unsigned int remaining = sizeof(m_buffer) - m_bufferSize;
	int size = read(m_socket, m_buffer + m_bufferSize, remaining);
	if (size <= 0)
	{
		delete this;
		return;
	}

	char *nl = reinterpret_cast<char *>(std::memchr(m_buffer + m_bufferSize, '\n', size));
	m_bufferSize += size;

	if (nl == 0)
	{
		if (m_overflow)
			m_bufferSize = 0;
		else if (m_bufferSize == sizeof(m_buffer))
			m_overflow = true;
	}
	else
	{
		do
		{
			*nl = 0;

			std::ptrdiff_t lineSize = nl - m_buffer;
			if (m_overflow)
			{
				m_overflow = false;
				SEND_RESP(RESP_INVALID_COMMAND);
			}
			else
				handleCommand(m_buffer, lineSize);

			if (lineSize + 1 < m_bufferSize)
			{
				std::memmove(m_buffer, nl + 1, m_bufferSize - size - 1);
				nl = reinterpret_cast<char *>(std::memchr(m_buffer, '\n', m_bufferSize));
			}
			else
			{
				m_bufferSize = 0;
				break;
			}
		} while (nl != 0 && m_bufferSize != 0);
	}
}

void TelnetSession::handleCommand (char *command, unsigned int size)
{
	while (size > 0 && std::isspace(command[size - 1]))
		--size;
	command[size] = 0;

	std::vector<char *> args = splitCommand(command);

	if (args.size() == 0)
		return;

	onCommand(args);
}

void TelnetSession::onCommand (const std::vector<char *> &argv)
{
	if (std::strcmp(argv[0], "show") == 0)
		onShowCommand(argv);
	else if (std::strcmp(argv[0], "exit") == 0)
		delete this;
	else if (std::strcmp(argv[0], "help") == 0)
		SEND_RESP(RESP_HELP);
	else
		SEND_RESP(RESP_INVALID_COMMAND);
}

void TelnetSession::onShowCommand (const std::vector<char *> &argv)
{
	if (argv.size() == 1)
		SEND_RESP(RESP_SHOW);
	else if (std::strcmp(argv[1], "router") == 0)
		onShowRouterCommand(argv);
	else if (std::strcmp(argv[1], "stat") == 0)
		onShowStatCommand(argv);
	else
		SEND_RESP(RESP_INVALID_COMMAND);
}

void TelnetSession::onShowStatCommand (const std::vector<char *> &)
{
	sendFormatted("Router Checksum Errors:         %llu\n", (unsigned long long int)VrrpSocket::routerChecksumErrors());
	sendFormatted("Router Version Errors:          %llu\n", (unsigned long long int)VrrpSocket::routerVersionErrors());
	sendFormatted("Router Virtual Router Id Error: %llu\n", (unsigned long long int)VrrpSocket::routerVrIdErrors());
}

void TelnetSession::onShowRouterCommand (const std::vector<char *> &argv)
{
	int vrid;
	int interface;
	if (argv.size() == 2)
	{
		vrid = 0;
		interface = 0;
	}
	else if (std::strcmp(argv[2], "stat") == 0)
		onShowRouterStatCommand(argv);
	else if (std::strcmp(argv[2], "interface") == 0)
	{
		if (argv.size() == 3)
		{
			SEND_RESP(RESP_SHOW_ROUTER_INTERFACE);
			return;
		}

		interface = if_nametoindex(argv[3]);

		if (argv.size() == 4)
			vrid = 0;
		else
		{
			vrid = std::atoi(argv[2]);
			if (vrid == 0 || vrid > 255)
			{
				SEND_RESP(RESP_SHOW_ROUTER_INTERFACE);
				return;
			}
		}
	}
	else
	{
		vrid = std::atoi(argv[2]);
		if (vrid == 0 || vrid > 255)
		{
			SEND_RESP(RESP_SHOW_ROUTER);
			return;
		}
	}

	const VrrpManager::VrrpServiceMap &services = VrrpManager::services();
	if (interface > 0)
	{
		VrrpManager::VrrpServiceMap::const_iterator interfaceServices = services.find(interface);
		if (interfaceServices != services.end())
		{
			if (vrid == 0)
			{
				for (VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.begin(); services != interfaceServices->second.end(); ++services)
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
						showRouter(service->second);
				}
			}
			else
			{
				VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.find(vrid);
				if (services != interfaceServices->second.end())
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
						showRouter(service->second);
				}
			}
		}
	}
	else
	{
		for (VrrpManager::VrrpServiceMap::const_iterator interfaceServices = services.begin(); interfaceServices != services.end(); ++interfaceServices)
		{
			if (vrid == 0)
			{
				for (VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.begin(); services != interfaceServices->second.end(); ++services)
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
						showRouter(service->second);
				}
			}
			else
			{
				VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.find(vrid);
				if (services != interfaceServices->second.end())
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
						showRouter(service->second);
				}
			}
		}
	}
}

void TelnetSession::onShowRouterStatCommand (const std::vector<char *> &argv)
{
}

std::vector<char *> TelnetSession::splitCommand (char *command)
{
	std::vector<char *> args;
	bool whiteSpace = true;
	while (*command != 0)
	{
		if (std::isspace(*command))
		{
			if (!whiteSpace)
			{
				*command = 0;
				whiteSpace = true;
			}
		}
		else
		{
			if (whiteSpace)
			{
				args.push_back(command);
				whiteSpace = false;
			}
		}

		++command;
	}
	return args;
}

void TelnetSession::sendFormatted (const char *templ, ...)
{
	std::va_list args;
	va_start(args, templ);
	int size = std::vsprintf(m_buffer, templ, args);
	va_end(args);

	write(m_socket, m_buffer, size);
}

void TelnetSession::showRouter (const VrrpService *service)
{
	char tmp[IFNAMSIZ];
	sendFormatted("Virtual router %hhu on interface %s (%s)\n", service->virtualRouterId(), if_indextoname(service->interface(), tmp), service->family() == AF_INET ? "IPv4" : "IPv6");
	sendFormatted(" Master IP Address:      %s\n", service->masterIpAddress().toString().c_str());
	sendFormatted(" Primary IP Address:     %s\n", service->primaryIpAddress().toString().c_str());
	sendFormatted(" Virtual MAC:            00:00:5e:00:%02u:%02hhu\n", service->family() == AF_INET ? 1 : 2, (unsigned char)service->virtualRouterId());

	static const char *states[] = {"Initialize", "Backup", "Master"};
	sendFormatted(" Status:                 %s\n", states[service->state() - 1]);

	sendFormatted(" Priority:               %u\n", (unsigned int)service->priority());
	sendFormatted(" Advertisement Interval: %u msec\n", (unsigned int)service->advertisementInterval() * 10);
	sendFormatted(" Preempt Mode:           %s\n", service->preemptMode() ? "Yes" : "No");
	sendFormatted(" Accept Mode:            %s\n", service->acceptMode() ? "Yes" : "No");
	SEND_RESP(" Address List:\n");

	const IpAddressList list = service->addresses();
	for (IpAddressList::const_iterator addr = list.begin(); addr != list.end(); ++addr)
		sendFormatted("  %s\n", addr->toString().c_str());

	SEND_RESP("\n");
}
