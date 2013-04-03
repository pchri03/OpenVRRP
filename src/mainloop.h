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

#ifndef INCLUDE_MAIN_LOOP_H
#define INCLUDE_MAIN_LOOP_H

#include <map>

class MainLoop
{
	public:
		typedef void (Callback)(int fd, void *userData);

		static bool addMonitor (int fd, Callback *callback, void *userData);
		static bool removeMonitor (int fd);

		static bool run ();

	private:
		static void init ();
		static void timerCallback (int fd, Callback *callback, void *userData);
		static void signalCallback (int signum);

	private:
		struct Monitor
		{
			Callback *callback;
			void *userData;
			int fd;
		};

		struct Timer
		{
			Callback *callback;
			void *userData;
		};

		typedef std::map<int, Monitor *> MonitorMap;
		typedef std::map<int, Timer *> TimerMap;
		
		static int m_fd;
		static MonitorMap m_monitors;
		static TimerMap m_timers;
		static bool m_aborted;
		static int m_abortSignal;
};

#endif
