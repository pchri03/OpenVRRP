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

#ifndef INCLUDE_OPENVRRP_VRRPSERVICE_H
#define INCLUDE_OPENVRRP_VRRPSERVICE_H

#include "ipaddress.h"
#include "timer.h"
#include "vrrpeventlistener.h"

#include <cstdint>

class VrrpSocket;

class VrrpService : private VrrpEventListener
{
	public:
		enum State
		{
			Initialize,
			Backup,
			Master
		};

		VrrpService (int interface, int family, const IpAddress &primaryIpAddress, std::uint_fast8_t virtualRouterId, std::uint_fast8_t priority = 100);
		~VrrpService ();

		inline std::uint_fast8_t virtualRouterId () const
		{
			return m_virtualRouterId;
		}

		inline std::uint_fast8_t priority () const
		{
			return m_priority;
		}

		inline void setPriority (std::uint_fast8_t priority)
		{
			m_priority = priority;
		}

		inline unsigned int advertisementInterval () const
		{
			return m_advertisementInterval;
		}

		inline void setAdvertisementInterval (unsigned int advertisementInterval)
		{
			m_advertisementInterval = advertisementInterval;
		}

		inline unsigned int masterAdvertisementInterval () const
		{
			return m_masterAdvertisementInterval;
		}

		inline unsigned int skewTime () const
		{
			return ((256 - m_priority) * m_masterAdvertisementInterval) / 256;
		}
		
		inline unsigned int masterDownInterval () const
		{
			return 3 * m_masterAdvertisementInterval + skewTime();
		}
		
		inline bool preemptMode () const
		{
			return m_preemptMode;
		}

		inline void setPreemptMode (bool enabled)
		{
			m_preemptMode = enabled;
		}

		inline bool acceptMode () const
		{
			return m_acceptMode;
		}

		inline void setAcceptMode (bool enabled)
		{
			m_acceptMode = enabled;
		}

		bool addIpAddress (const IpAddress &address);
		bool removeIpAddress (const IpAddress &address);
		const IpAddressList &addresses () const;

		inline State state () const
		{
			return m_state;
		}

		void startup ();
		void shutdown ();

	private:
		virtual void onIncomingVrrpPacket (
				unsigned int interface,
				const IpAddress &address,
				std::uint_fast8_t virtualRouterId,
				std::uint_fast8_t priority,
				std::uint_fast16_t maxAdvertisementInterval,
				const IpAddressList &addresses);

		void onMasterDownTimer ();
		void onAdvertisementTimer ();

		bool sendAdvertisement (std::uint_least8_t priority);
		void sendARPs();
		void sendNeighborAdvertisements();
		void joinSolicitedNodeMulticast();
		bool setVirtualMac();
		bool setDefaultMac();
		void setState (State state);
		bool addIpAddresses ();
		bool removeIpAddresses ();

		static void timerCallback (Timer *timer, void *userData);

	private:
		std::uint_fast8_t m_virtualRouterId;
		std::uint_fast8_t m_priority;
		IpAddress m_primaryIpAddress;
		unsigned int m_advertisementInterval;
		unsigned int m_masterAdvertisementInterval;
		bool m_preemptMode;
		bool m_acceptMode;

		Timer m_masterDownTimer;
		Timer m_advertisementTimer;

		State m_state;

		int m_family;
		int m_interface;
		int m_outputInterface;

		std::uint8_t m_mac[6];

		IpAddressList m_addresses;

		VrrpSocket *m_socket;

		const char *m_name;
};

#endif // INCLUDE_OPENVRRP_VRRPSERVICE_H
