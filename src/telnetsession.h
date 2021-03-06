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

#ifndef INCLUDE_TELNETSESSION_H
#define INCLUDE_TELNETSESSION_H

#include "ipaddress.h"

#include <cstdint>
#include <vector>

class TelnetServer;
class VrrpService;

class TelnetSession
{
	public:
		TelnetSession (int fd, TelnetServer *server);
		~TelnetSession ();

	private:
		static void onIncomingData (int fd, void *userData);
		void receiveChunk ();
		bool handleCommand (char *command, unsigned int size);
		bool onCommand (const std::vector<char *> &argv);

		void onAddCommand (const std::vector<char *> &argv);
		void onAddRouterCommand (const std::vector<char *> &argv);
		void onAddAddressCommand (const std::vector<char *> &argv);

		void onRemoveCommand (const std::vector<char *> &argv);
		void onRemoveRouterCommand (const std::vector<char *> &argv);
		void onRemoveAddressCommand (const std::vector<char *> &argv);

		void onSetCommand (const std::vector<char *> &argv);
		void onSetRouterCommand (const std::vector<char *> &argv);
		void onEnableCommand (const std::vector<char *> &argv);
		void onDisableCommand (const std::vector<char *> &argv);
		void onShowCommand (const std::vector<char *> &argv);

		void onShowRouterCommand (const std::vector<char *> &argv);
		void onShowStatsCommand (const std::vector<char *> &argv);

		void onSaveCommand (const std::vector<char *> &argv);
		
		void showRouter (const VrrpService *service);
		void showRouterStats (const VrrpService *service);

		void sendFormatted (const char *templ, ...);

		static std::vector<char *> splitCommand (char *command);

		VrrpService *getService (const std::vector<char *> &argv, bool create = false);

	private:
		int m_socket;
		unsigned int m_bufferSize;
		TelnetServer *m_server;
		char m_buffer[1024];
		bool m_overflow;
};

#endif
