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

#include "timer.h"
#include "mainloop.h"

#include <cerrno>

#include <unistd.h>
#include <syslog.h>
#include <sys/timerfd.h>

Timer::Timer (Callback *callback, void *userData) :
	m_fd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
	m_callback(callback),
	m_userData(userData),
	m_armed(false)
{
	if (m_fd == -1)
		syslog(LOG_ERR, "Error creating timer: %m");
}

Timer::~Timer ()
{
	stop();
	while (close(m_fd) == -1 && errno == EINTR);
}

void Timer::start (unsigned int msec)
{
	struct itimerspec value;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_nsec = 0;
	value.it_value.tv_sec = msec / 1000;
	value.it_value.tv_nsec = (msec % 1000) * 1000000;
	timerfd_settime(m_fd, 0, &value, 0);
	if (!m_armed)
	{
		m_armed = true;
		MainLoop::addMonitor(m_fd, callback, this);
	}
}

void Timer::stop ()
{
	if (m_armed)
	{
		struct itimerspec value;
		value.it_interval.tv_sec = 0;
		value.it_interval.tv_nsec = 0;
		value.it_value.tv_sec = 0;
		value.it_value.tv_nsec = 0;
		timerfd_settime(m_fd, TFD_TIMER_ABSTIME, &value, 0);
		MainLoop::removeMonitor(m_fd);
		m_armed = false;
	}
}

void Timer::callback (int, void *userData)
{
	Timer *self = reinterpret_cast<Timer *>(userData);
	if (self->m_armed)
	{
		self->stop();
		self->m_callback(self, self->m_userData);
	}
}
