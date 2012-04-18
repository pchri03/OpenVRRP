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

#ifndef INCLUDE_VRRP_H
#define INCLUDE_VRRP_H

#include "timer.h"

#include <cstdint>

class Vrrp
{
	public:
		Vrrp (const char *interface, int family, std::uint8_t virtualRouterId, std::uint8_t priority = 100);
		~Vrrp();

		std::uint8_t virtualRouterId () const;
		
		std::uint8_t priority () const;
		void setPriority (std::uint8_t priority);

		unsigned int advertisementInterval () const;
		void setAdvertisementInterval (unsigned int advertisementInterval);

		unsigned int masterAdvertisementInterval () const;
		void setMasterAdvertisementInterval (unsigned int masterAdvertisementInterval);

		unsigned int skewTime () const;
		unsigned int masterDownInterval () const;

		bool getPreemptMode () const;
		void setPreemptMode (bool enabled);

		bool getAcceptMode () const;
		void setAcceptMode (bool enabled);

	protected:
		enum State
		{
			Initialize,
			Backup,
			Master
		};

		void initSocket (const char *interface, int family);

		void startup ();
		void shutdown ();
		virtual void onStartupEvent () = 0;
		virtual void onMasterDownEvent () = 0;
		virtual void sendAdvertisement (int priority = -1) = 0;
		virtual void onVrrpPacket (const std::uint8_t *packet, unsigned int size) = 0;

		void setState (State state);

	private:
		static void socketCallback (int, void *);
		static void masterDownTimerCallback (Timer *, void *);
		static void advertisementTimerCallback (Timer *, void *);

	private:
		std::uint8_t m_virtualRouterId;
		std::uint8_t m_priority;
		unsigned int m_advertisementInterval;
		unsigned int m_masterAdvertisementInterval;
		bool m_preemptMode;
		bool m_acceptMode;

		Timer m_masterDownTimer;
		Timer m_advertisementTimer;

		State m_state;
		
		int m_socket;

		std::uint8_t m_buffer[1500];
};

#endif // INCLUDE_VRRP_H
