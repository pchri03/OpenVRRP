/*
 * Copyright (C) 2013 Peter Christensen <pch@ordbogen.com>
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

#include "scriptrunner.h"

#include <string>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <syslog.h>
#include <signal.h>

void ScriptRunner::execute (const std::string &command)
{
	static bool initialized = false;
	if (!initialized)
		signal(SIGCHLD, SIG_IGN);

	pid_t pid = fork();
	if (pid == 0)
	{
		// Child process
		execl("/bin/sh", "sh", "-c", command.c_str(), 0);
		syslog(LOG_ERR, "Error executing command `%s': %s", command.c_str(), std::strerror(errno));
		_exit(EXIT_FAILURE);
	}
	else
	{
		syslog(LOG_INFO, "Executed command: %s", command.c_str());
	}
}
