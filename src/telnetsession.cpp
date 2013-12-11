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
#include "configurator.h"

#include <cstring>
#include <cstdarg>
#include <cstdio>

#include <unistd.h>
#include <syslog.h>
#include <net/if.h>

#define RESP_INVALID_COMMAND	"Invalid command\n"
#define RESP_NO_SUCH_INTERFACE	"No such interface\n"
#define RESP_INVALID_ROUTER_ID	"Invalid router id\n"
#define RESP_ERROR_CREATING_ROUTER	"Error creating router\n"
#define RESP_NO_SUCH_ROUTER		"No such router\n"
#define RESP_INVALID_IP			"Invalid ip address\n"
#define RESP_INVALID_PRIORITY	"Invalid priority\n"
#define RESP_INVALID_INTERVAL	"Invalid interval\n"

#define RESP_ADD_ROUTER				"add router INTF VRID [ipv6]\n"
#define RESP_ADD_ADDRESS			"add address INTF VRID [ipv6] CIDR\n"
#define RESP_REMOVE_ROUTER			"remove router INTF VRID [ipv6]\n"
#define RESP_REMOVE_ADDRESS			"remove address INTF VRID [ipv6] CIDR\n"
#define RESP_SET_ROUTER_PRIMARY		"set router INTF VRID [ipv6] primary IP\n"
#define RESP_SET_ROUTER_PRIORITY	"set router INTF VRID [ipv6] priority PRIO\n"
#define RESP_SET_ROUTER_INTERVAL	"set router INTF VRID [ipv6] interval MSEC\n"
#define RESP_SET_ROUTER_ACCEPT		"set router INTF VRID [ipv6] accept BOOL\n"
#define RESP_SET_ROUTER_PREEMPT		"set router INTF VRID [ipv6] preempt BOOL\n"
#define RESP_SET_ROUTER_STATUS		"set router INTF VRID [ipv6] status [master|slave]\n"
#define RESP_SET_ROUTER_MASTER_CMD	"set router INTF VRID [ipv6] master command COMMAND\n"
#define RESP_SET_ROUTER_BACKUP_CMD	"set router INTF VRID [ipv6] backup command COMMAND\n"
#define RESP_ENABLE_ROUTER			"enable router INTF VRID [ipv6]\n"
#define RESP_DISABLE_ROUTER			"disable router INTF VRID [ipv6]\n"
#define RESP_SHOW_ROUTER			"show router [INTF] [VRID] [ipv6] [stats]\n"
#define RESP_SHOW_STATS				"show stats\n"
#define RESP_SAVE					"save [FILENAME]\n"

#define RESP_ADD					RESP_ADD_ROUTER \
									RESP_ADD_ADDRESS

#define RESP_REMOVE					RESP_REMOVE_ROUTER \
									RESP_REMOVE_ADDRESS

#define RESP_SET_ROUTER				RESP_SET_ROUTER_ACCEPT \
									RESP_SET_ROUTER_INTERVAL \
									RESP_SET_ROUTER_PREEMPT \
									RESP_SET_ROUTER_PRIMARY \
									RESP_SET_ROUTER_PRIORITY \
									RESP_SET_ROUTER_STATUS \
									RESP_SET_ROUTER_MASTER_CMD \
									RESP_SET_ROUTER_BACKUP_CMD

#define RESP_SET					RESP_SET_ROUTER

#define RESP_ENABLE					RESP_ENABLE_ROUTER

#define RESP_DISABLE				RESP_DISABLE_ROUTER

#define RESP_SHOW					RESP_SHOW_ROUTER \
									RESP_SHOW_STATS

#define RESP_HELP					RESP_ADD \
									RESP_DISABLE \
									RESP_ENABLE  \
									"exit\n" \
									"help\n" \
									RESP_REMOVE \
									RESP_SET \
									RESP_SAVE

#define RESP_PROMPT				"OpenVRRP> "

#define SEND_RESP(str)	write(m_socket, str, sizeof(str) -1)

TelnetSession::TelnetSession (int fd, TelnetServer *server) :
	m_socket(fd),
	m_bufferSize(0),
	m_server(server),
	m_overflow(false)
{
	MainLoop::addMonitor(m_socket, onIncomingData, this);
	SEND_RESP(RESP_PROMPT);
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
			{
				if (!handleCommand(m_buffer, lineSize))
				{
					delete this;
					break;
				}
			}

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

bool TelnetSession::handleCommand (char *command, unsigned int size)
{
	while (size > 0 && std::isspace(command[size - 1]))
		--size;
	command[size] = 0;

	std::vector<char *> args = splitCommand(command);

	if (args.size() > 0)
	{
		if (!onCommand(args))
			return false;
	}

	SEND_RESP(RESP_PROMPT);
	return true;
}

bool TelnetSession::onCommand (const std::vector<char *> &argv)
{
	if (std::strcmp(argv[0], "add") == 0)
		onAddCommand(argv);
	else if (std::strcmp(argv[0], "remove") == 0)
		onRemoveCommand(argv);
	else if (std::strcmp(argv[0], "set") == 0)
		onSetCommand(argv);
	else if (std::strcmp(argv[0], "enable") == 0)
		onEnableCommand(argv);
	else if (std::strcmp(argv[0], "disable") == 0)
		onDisableCommand(argv);
	else if (std::strcmp(argv[0], "show") == 0)
		onShowCommand(argv);
	else if (std::strcmp(argv[0], "exit") == 0)
		return false;
	else if (std::strcmp(argv[0], "help") == 0)
		SEND_RESP(RESP_HELP);
	else if (std::strcmp(argv[0], "save") == 0)
		onSaveCommand(argv);
	else
		SEND_RESP(RESP_INVALID_COMMAND);

	return true;
}

void TelnetSession::onAddCommand (const std::vector<char *> &argv)
{
	if (argv.size() == 1)
		SEND_RESP(RESP_ADD);
	else if (std::strcmp(argv[1], "router") == 0)
		onAddRouterCommand(argv);
	else if (std::strcmp(argv[1], "address") == 0)
		onAddAddressCommand(argv);
	else
		SEND_RESP(RESP_ADD);
}

void TelnetSession::onAddRouterCommand (const std::vector<char *> &argv)
{
	bool ipv6 = (argv.size() > 4 && std::strcmp(argv[4], "ipv6") == 0);

	if (ipv6)
	{
		if (argv.size() == 5)
		{
			// add router INTF VRID ipv6
			getService(argv, true);
		}
		else
			SEND_RESP(RESP_ADD_ROUTER);
	}
	else
	{
		if (argv.size() == 4)
		{
			// add router INTF VRID
			getService(argv, true);
		}
		else
			SEND_RESP(RESP_ADD_ROUTER);
	}
}

void TelnetSession::onAddAddressCommand (const std::vector<char *> &argv)
{
	bool ipv6 = (argv.size() > 4 && std::strcmp(argv[4], "ipv6") == 0);

	if (ipv6)
	{
		if (argv.size() == 6)
		{
			// add address INTF VRID ipv6 IP

			IpSubnet subnet(argv[5]);
			if (subnet.address().family() != AF_INET6)
			{
				SEND_RESP(RESP_ADD_ADDRESS);
				return;
			}			

			VrrpService *service = getService(argv);
			if (service != 0)
				service->addIpAddress(subnet);
		}
		else
			SEND_RESP(RESP_ADD_ADDRESS);
	}
	else
	{
		if (argv.size() == 5)
		{
			// add address INTF VRID IP

			IpSubnet subnet(argv[4]);
			if (subnet.address().family() != AF_INET)
			{
				SEND_RESP(RESP_ADD_ADDRESS);
				return;
			}

			VrrpService *service = getService(argv);
			if (service != 0)
				service->addIpAddress(subnet);
		}
		else
			SEND_RESP(RESP_ADD_ADDRESS);
	}

}

void TelnetSession::onRemoveCommand (const std::vector<char *> &argv)
{
	if (argv.size() == 1)
		SEND_RESP(RESP_REMOVE);
	else if (std::strcmp(argv[1], "router") == 0)
		onRemoveRouterCommand(argv);
	else if (std::strcmp(argv[1], "address") == 0)
		onRemoveAddressCommand(argv);
	else
		SEND_RESP(RESP_REMOVE);
}

void TelnetSession::onRemoveRouterCommand (const std::vector<char *> &argv)
{
	bool ipv6 = (argv.size() > 4 && std::strcmp(argv[4], "ipv6") == 0);

	if (ipv6)
	{
		if (argv.size() == 5)
		{
			// remove router INTF VRID ipv6
			VrrpService *service = getService(argv);
			if (service != 0)
				VrrpManager::removeService(service);
		}
		else
			SEND_RESP(RESP_REMOVE_ROUTER);
	}
	else
	{
		if (argv.size() == 4)
		{
			// remove router INTF VRID
			VrrpService *service = getService(argv);
			if (service != 0)
				VrrpManager::removeService(service);
		}
		else
			SEND_RESP(RESP_REMOVE_ROUTER);
	}
}

void TelnetSession::onRemoveAddressCommand (const std::vector<char *> &argv)
{
	bool ipv6 = (argv.size() > 4 && std::strcmp(argv[4], "ipv6") == 0);

	if (ipv6)
	{
		if (argv.size() == 6)
		{
			// remove address INTF VRID ipv6 IP

			IpSubnet subnet(argv[5]);
			if (subnet.address().family() != AF_INET6)
			{
				SEND_RESP(RESP_REMOVE_ADDRESS);
				return;
			}			

			VrrpService *service = getService(argv);
			if (service != 0)
				service->removeIpAddress(subnet);
		}
		else
			SEND_RESP(RESP_REMOVE_ADDRESS);
	}
	else
	{
		if (argv.size() == 5)
		{
			// remove address INTF VRID IP

			IpSubnet subnet(argv[4]);
			if (subnet.address().family() != AF_INET)
			{
				SEND_RESP(RESP_REMOVE_ADDRESS);
				return;
			}

			VrrpService *service = getService(argv);
			if (service != 0)
				service->removeIpAddress(subnet);
		}
		else
			SEND_RESP(RESP_REMOVE_ADDRESS);
	}
}

void TelnetSession::onSetCommand (const std::vector<char *> &argv)
{
	if (argv.size() > 1 && std::strcmp(argv[1], "router") == 0)
	{
		onSetRouterCommand(argv);
		return;
	}

	SEND_RESP(RESP_SET);
}

void TelnetSession::onSetRouterCommand (const std::vector<char *> &argv)
{
	static const char *trueValues[] = {"true", "on", "1", "enabled"};
	static const char *falseValues[] = {"false", "off", "0", "disabled"};

	VrrpService *service = getService(argv);
	if (service != 0)
	{
		int offset = (service->family() == AF_INET6 ? 5 : 4);
		if (argv.size() > offset)
		{
			if (std::strcmp(argv[offset], "primary") == 0)
			{
				if (argv.size() > offset + 1)
				{
					IpAddress address(argv[offset + 1]);
					if (address.family() != service->family())
					{
						SEND_RESP(RESP_INVALID_IP);
						return;
					}

					service->setPrimaryIpAddress(address);
					return;
				}
				
				SEND_RESP(RESP_SET_ROUTER_PRIMARY);
				return;
			}
			else if (std::strcmp(argv[offset], "priority") == 0)
			{
				if (argv.size() > offset + 1)
				{
					int priority = std::atoi(argv[offset + 1]);
					if (priority < 1 || priority > 255)
					{
						SEND_RESP(RESP_INVALID_PRIORITY);
						return;
					}

					service->setPriority(priority);
					return;
				}

				SEND_RESP(RESP_SET_ROUTER_PRIORITY);
				return;
			}
			else if (std::strcmp(argv[offset], "interval") == 0)
			{
				if (argv.size() > offset + 1)
				{
					int interval = std::atoi(argv[offset + 1]);
					if (interval == 0 || interval % 10 != 0 || interval > 40950)
					{
						SEND_RESP(RESP_INVALID_INTERVAL);
						return;
					}

					service->setAdvertisementInterval(interval / 10);
					return;
				}

				SEND_RESP(RESP_SET_ROUTER_INTERVAL);
				return;
			}
			else if (std::strcmp(argv[offset], "accept") == 0)
			{
				if (argv.size() > offset + 1)
				{
					for (int i = 0; i != sizeof(trueValues) / sizeof(trueValues[0]); ++i)
					{
						if (std::strcmp(argv[offset + 1], trueValues[i]) == 0)
						{
							service->setAcceptMode(true);
							return;
						}
					}

					for (int i = 0; i != sizeof(falseValues) / sizeof(falseValues[0]); ++i)
					{
						if (std::strcmp(argv[offset + 1], falseValues[i]) == 0)
						{
							service->setAcceptMode(false);
							return;
						}
					}
				}

				SEND_RESP(RESP_SET_ROUTER_ACCEPT);
				return;
			}
			else if (std::strcmp(argv[offset], "preempt") == 0)
			{
				if (argv.size() > offset + 1)
				{
					for (int i = 0; i != sizeof(trueValues) / sizeof(trueValues[0]); ++i)
					{
						if (std::strcmp(argv[offset + 1], trueValues[i]) == 0)
						{
							service->setPreemptMode(true);
							return;
						}
					}

					for (int i = 0; i != sizeof(falseValues) / sizeof(falseValues[0]); ++i)
					{
						if (std::strcmp(argv[offset + 1], falseValues[i]) == 0)
						{
							service->setPreemptMode(false);
							return;
						}
					}
				}

				SEND_RESP(RESP_SET_ROUTER_PREEMPT);
				return;

			}
			else if (std::strcmp(argv[offset], "master") == 0 || std::strcmp(argv[offset], "backup") == 0)
			{
				if (argv.size() > offset + 1 && std::strcmp(argv[offset + 1], "command") == 0)
				{
					std::string command;
					for (int i = offset + 2; i != argv.size(); ++i)
					{
						if (i != offset + 2)
							command += ' ';
						command += argv[i];
					}

					if (std::strcmp(argv[offset], "master") == 0)
						service->setMasterCommand(command);
					else // if (std::strcmp(argv[offset], "backup") == 0)
						service->setBackupCommand(command);
					return;
				}
				else if (std::strcmp(argv[offset], "master") == 0)
				{
					SEND_RESP(RESP_SET_ROUTER_MASTER_CMD);
					return;
				}
				else // if (std::strcmp(argv[offset], "backup") == 0)
				{
					SEND_RESP(RESP_SET_ROUTER_BACKUP_CMD);
					return;
				}
			}
			else if (std::strcmp(argv[offset], "status") == 0)
			{
				// TODO
			}
		}
			
		SEND_RESP(RESP_SET);
	}
}

void TelnetSession::onEnableCommand (const std::vector<char *> &argv)
{
	if (argv.size() < 4)
	{
		SEND_RESP(RESP_ENABLE_ROUTER);
		return;
	}

	VrrpService *service = getService(argv);
	if (service != 0)
		service->enable();
}

void TelnetSession::onDisableCommand (const std::vector<char *> &argv)
{
	if (argv.size() < 4)
	{
		SEND_RESP(RESP_DISABLE_ROUTER);
		return;
	}

	VrrpService *service = getService(argv);
	if (service != 0)
		service->disable();
}

void TelnetSession::onShowCommand (const std::vector<char *> &argv)
{
	if (argv.size() > 1)
	{
		if (std::strcmp(argv[1], "router") == 0)
		{
			onShowRouterCommand(argv);
			return;
		}
		else if (std::strcmp(argv[1], "stats") == 0)
		{
			onShowStatsCommand(argv);
			return;
		}
	}

	SEND_RESP(RESP_SHOW);
}

void TelnetSession::onShowRouterCommand (const std::vector<char *> &argv)
{
	int interface = -1;
	int vrid = -1;
	int family = AF_INET;
	bool stats = false;

	for (int i = 2; i < argv.size(); ++i)
	{
		if (std::strcmp(argv[i], "stats") == 0)
		{
			stats = true;
			break;
		}
		else if (family == AF_UNSPEC && std::strcmp(argv[i], "ipv6") == 0)
		{
			if (interface == -1)
				interface = 0;
			if (vrid == -1)
				vrid = 0;

			family = AF_INET6;
		}
		else if (vrid == -1 && std::isdigit(argv[i][0]))
		{
			if (interface == -1)
				interface = 0;

			vrid = std::atoi(argv[i]);
			if (vrid <= 0 || vrid > 255)
			{
				SEND_RESP(RESP_INVALID_ROUTER_ID);
				return;
			}
		}
		else if (interface == -1)
		{
			interface = if_nametoindex(argv[i]);
			if (interface <= 0)
			{
				SEND_RESP(RESP_NO_SUCH_INTERFACE);
				return;
			}
		}
	}

	if (interface == -1)
		interface = 0;
	if (vrid == -1)
		vrid = 0;

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
					{
						if (stats)
							showRouterStats(service->second);
						else
							showRouter(service->second);
					}
				}
			}
			else
			{
				VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.find(vrid);
				if (services != interfaceServices->second.end())
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
					{
						if (stats)
							showRouterStats(service->second);
						else
							showRouter(service->second);
					}
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
					{
						if (stats)
							showRouterStats(service->second);
						else
							showRouter(service->second);
					}
				}
			}
			else
			{
				VrrpManager::VrrpServiceMap::mapped_type::const_iterator services = interfaceServices->second.find(vrid);
				if (services != interfaceServices->second.end())
				{
					for (VrrpManager::VrrpServiceMap::mapped_type::mapped_type::const_iterator service = services->second.begin(); service != services->second.end(); ++service)
					{
						if (stats)
							showRouterStats(service->second);
						else
							showRouter(service->second);
					}
				}
			}
		}
	}
}

void TelnetSession::onShowStatsCommand (const std::vector<char *> &)
{
	sendFormatted("Router Checksum Errors: %llu\n", (unsigned long long int)VrrpSocket::routerChecksumErrors());
	sendFormatted("Router Version Errors:  %llu\n", (unsigned long long int)VrrpSocket::routerVersionErrors());
	sendFormatted("Router VRID Errors:     %lu\n", (unsigned long long int)VrrpSocket::routerVrIdErrors());
	SEND_RESP("\n");
}

VrrpService *TelnetSession::getService (const std::vector<char *> &argv, bool create)
{
	// xxx xxx INTF VRID [ipv6]
	int interface = if_nametoindex(argv[2]);
	if (interface <= 0)
	{
		SEND_RESP(RESP_NO_SUCH_INTERFACE);
		return 0;
	}

	int vrid = std::atoi(argv[3]);
	if (vrid <= 0 || vrid > 255)
	{
		SEND_RESP(RESP_INVALID_ROUTER_ID);
		return 0;
	}

	int family = (argv.size() > 4 && std::strcmp(argv[4], "ipv6") == 0 ? AF_INET6 : AF_INET);

	VrrpService *service = VrrpManager::getService(interface, vrid, family, create);
	if (service == 0)
	{
		if (create)
			SEND_RESP(RESP_ERROR_CREATING_ROUTER);
		else
			SEND_RESP(RESP_NO_SUCH_ROUTER);
	}
	return service;
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
	sendFormatted(" Primary IP Address:     %s%s\n", service->primaryIpAddress().toString().c_str(), service->hasAutoPrimaryIpAddress() ? "" : " (Forced)");

	const std::uint8_t *mac = service->mac();
	sendFormatted(" Virtual MAC:            %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	static const char *states[] = {"Disabled", "Link Down", "Backup", "Master"};
	sendFormatted(" Status:                 %s\n", states[service->state()]);

	sendFormatted(" Priority:               %u\n", (unsigned int)service->priority());
	sendFormatted(" Advertisement Interval: %u msec\n", (unsigned int)service->advertisementInterval() * 10);
	sendFormatted(" Preempt Mode:           %s\n", service->preemptMode() ? "Yes" : "No");
	sendFormatted(" Accept Mode:            %s\n", service->acceptMode() ? "Yes" : "No");
	sendFormatted(" Master Command:         %s\n", service->masterCommand().c_str());
	sendFormatted(" Backup Command:         %s\n", service->backupCommand().c_str());
	SEND_RESP(" Address List:\n");

	const IpSubnetSet set = service->subnets();
	for (IpSubnetSet::const_iterator subnet = set.begin(); subnet != set.end(); ++subnet)
		sendFormatted("  %s\n", subnet->toString().c_str());

	SEND_RESP("\n");
}

void TelnetSession::showRouterStats (const VrrpService *service)
{
	char tmp[IFNAMSIZ];
	sendFormatted("Virtual router %hhu on interface %s (%s)\n", service->virtualRouterId(), if_indextoname(service->interface(), tmp), service->family() == AF_INET ? "IPv4" : "IPv6");
	sendFormatted(" Master Transitions:                    %u\n", service->statsMasterTransitions());

	static const char *reasons[] = {"Not master", "Priority", "Preempted", "Master not responding"};
	sendFormatted(" Master Reason:                         %s\n", reasons[service->statsNewMasterReason() - 1]);

	sendFormatted(" Received Advertisements:               %llu\n", (unsigned long long int)service->statsRcvdAdvertisements());
	sendFormatted(" Advertisement Interval Errors:         %llu\n", (unsigned long long int)service->statsAdvIntervalErrors());
	sendFormatted(" IP TTL Errors:                         %llu\n", (unsigned long long int)service->statsIpTtlErrors());

	static const char *errors[] = {"No error", "IP TTL error", "Version error", "Checksum error", "Virtual router ID error"};
	sendFormatted(" Protocol Error:                        %s\n", errors[service->statsProtocolErrReason()]);

	sendFormatted(" Priority Zero Advertisements Received: %llu\n", (unsigned long long int)service->statsRcvdPriZeroPackets());
	sendFormatted(" Priority Zero Advertisements Sent:     %llu\n", (unsigned long long int)service->statsSentPriZeroPackets());
	sendFormatted(" Invalid Packet Types Received:         %llu\n", (unsigned long long int)service->statsRcvdInvalidTypePackets());
	sendFormatted(" Address List Errors:                   %llu\n", (unsigned long long int)service->statsAddressListErrors());
	sendFormatted(" Packet Length Errors:                  %llu\n", (unsigned long long int)service->statsPacketLengthErrors());
	SEND_RESP("\n");
}

void TelnetSession::onSaveCommand (const std::vector<char *> &argv)
{
	if (Configurator::writeConfiguration(argv.size() > 1 ? argv[1] : 0))
		SEND_RESP("Save successfully\n");
	else
		SEND_RESP("Save failed\n");
}

