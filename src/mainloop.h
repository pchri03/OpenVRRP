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

class MainLoop
{
	public:
		MainLoop();
		~MainLoop();

		typedef void (Callback)(MainLoop &mainLoop, int fdOrTimerId, void *userData);

		void addMonitor (int fd, Callback *callback, void *userData);
		void removeMonitor (int fd);

		int addTimer (unsigned int msec);
		void removeTimer (int timerId);

		static MainLoop *globalInstance ();

	private:
		int m_epollFd;
		static MainLoop *m_globalInstance;
		static pthread_rwlock_t m_globalInstanceLock;
};

#endif
