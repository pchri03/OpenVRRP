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

#include <cerrno>
#include <cstring>

#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/epoll.h>

int MainLoop::m_fd = -1;
MainLoop::MonitorMap MainLoop::m_monitors;
MainLoop::TimerMap MainLoop::m_timers;
bool MainLoop::m_aborted = false;
int MainLoop::m_abortSignal = 0;

void MainLoop::init ()
{
	if (m_fd == -1)
		m_fd = epoll_create(32);
}

bool MainLoop::addMonitor (int fd, Callback *callback, void *userData)
{
	init();

	Monitor *monitor = new Monitor();
	monitor->callback = callback;
	monitor->userData = userData;
	monitor->fd = fd;

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = monitor;

	if (epoll_ctl(m_fd, EPOLL_CTL_ADD, fd, &event) == -1)
	{
		syslog(LOG_ERR, "MainLoop: Error adding file to main loop monitor: %s", std::strerror(errno));
		delete monitor;
		return false;
	}

	MonitorMap::iterator it = m_monitors.find(fd);
	if (it != m_monitors.end())
	{
		delete it->second;
		it->second = monitor;
	}
	else
		m_monitors[fd] = monitor;

	return true;
}

bool MainLoop::removeMonitor (int fd)
{
	MonitorMap::iterator it = m_monitors.find(fd);
	if (it == m_monitors.end())
		return false;

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = it->second;
	if (epoll_ctl(m_fd, EPOLL_CTL_DEL, fd, &event) == -1)
		syslog(LOG_ERR, "MainLoop: Error removing file from main loop monitor: %s", std::strerror(errno));

	delete it->second;
	m_monitors.erase(it);
	
	return true;
}

bool MainLoop::run ()
{
	bool ret = true;

	m_aborted = false;

	sighandler_t intHandler = signal(SIGINT, signalCallback);
	sighandler_t termHandler = signal(SIGTERM, signalCallback);
	sighandler_t quitHandler = signal(SIGQUIT, signalCallback);
	sighandler_t hupHandler = signal(SIGHUP, SIG_IGN);
	sighandler_t usr1Handler = signal(SIGUSR1, SIG_IGN);
	sighandler_t usr2Handler = signal(SIGUSR2, SIG_IGN);
	sighandler_t alarmHandler = signal(SIGALRM, SIG_IGN);

	while (m_monitors.size() > 0 && !m_aborted)
	{
		struct epoll_event events[16];
		int eventCount = epoll_wait(m_fd, events, sizeof(events) / sizeof(events[0]), -1);

		if (eventCount == -1)
		{
			if (errno != EINTR)
			{
				syslog(LOG_ERR, "MainLoop: epoll_wait failed: %s", std::strerror(errno));
				ret = false;
				break;
			}
			else
				syslog(LOG_NOTICE, "MainLoop: epoll_wait was interrupted by signal %s", strsignal(m_abortSignal));
		}
		else
		{
			for (int i = 0; i != eventCount; ++i)
			{
				Monitor *monitor = reinterpret_cast<Monitor *>(events[i].data.ptr);
				monitor->callback(monitor->fd, monitor->userData);
			}
		}
	}

	signal(SIGINT, intHandler);
	signal(SIGTERM, termHandler);
	signal(SIGQUIT, quitHandler);
	signal(SIGHUP, hupHandler);
	signal(SIGUSR1, usr1Handler);
	signal(SIGUSR2, usr2Handler);
	signal(SIGALRM, alarmHandler);

	return ret;
}

void MainLoop::signalCallback (int signum)
{
	m_abortSignal = signum;
	m_aborted = true;
	signal(signum, SIG_IGN);
}
