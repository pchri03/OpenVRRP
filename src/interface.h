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

#ifndef INCLUDE_INTERFACE_H
#define INCLUDE_INTERFACE_H

#include <cstdint>

class InterfaceEngine;

class Interface
{
	public:
		typedef void (Callback)(Interface &interface, const std::uint8_t *packet, unsigned int size, void *userData);

		virtual ~Interface ();

		virtual void setAddress (const uint8_t *mac) = 0;
		virtual void resetAddress () = 0;
		
		virtual void addCallback (Callback *callback, void *userData) = 0;
		virtual void removeCallback (Callback *callback) = 0;

		void send (const void *data, unsigned int size);

		int family () const;

		static Interface getInterface (const char *name, int family);

	private:
		InterfaceEngine *m_engine;
};

#endif // INCLUDE_INTERFACE_H
