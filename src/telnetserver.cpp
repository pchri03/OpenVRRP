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

#include "telnetserver.h"
#include "mainloop.h"
#include "telnetsession.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <syslog.h>

TelnetServer::TelnetServer (const IpAddress &address) :
	m_address(address),
	m_socket(-1)
{
}

TelnetServer::~TelnetServer ()
{
	stop();
}

bool TelnetServer::start ()
{
	m_socket = socket(m_address.family(), SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == -1)
	{
		syslog(LOG_ERR, "Error starting telnet server: %s", std::strerror(errno));
		return false;
	}

	if (bind(m_socket, m_address.socketAddress(), m_address.socketAddressSize()) == -1 || listen(m_socket, 10) == -1)
	{
		syslog(LOG_ERR, "Error starting telnet server: %s", std::strerror(errno));
		stop();
		return false;
	}

	if (!MainLoop::addMonitor(m_socket, onIncomingConnection, this))
	{
		stop();
		return false;
	}

	return true;
}

void TelnetServer::stop ()
{
	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		while (::close(m_socket) == -1 && errno == EINTR);
		m_socket = -1;
	}

	while (m_sessions.size() != 0)
		delete *m_sessions.begin();
}

void TelnetServer::onIncomingConnection (int fd, void *userData)
{
	int s = accept(fd, 0, 0);
	if (s != -1)
	{
		TelnetServer *self = reinterpret_cast<TelnetServer *>(userData);
		TelnetSession *session = new TelnetSession(s, self);
		self->m_sessions.insert(session);
	}
}

void TelnetServer::removeSession (TelnetSession *session)
{
	m_sessions.erase(session);
}
