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

#ifndef INCLUDE_ABSTRACT_INTERFACE_H
#define INCLUDE_ABSTRACT_INTERFACE_H

#include <cstdint>
#include <map>

class AbstractInterface
{
	public:
		typedef void (Callback)(AbstractInterface *, const std::uint8_t *packet, unsigned int size, void *userData);

		virtual ~AbstractInterface ();

		void addCallback (Callback *callback, void *userData);
		void removeCallback (Callback *callback);

		virtual void setAddress (const uint8_t *mac);
		virtual void resetAddress ();

	private:
		typedef std::map<Callback *, void *> CallbackMap;

	private:
		CallbackMap m_callbacks;
};

#endif // INCLUDE_ABSTRACT_INTERFACE_H
