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
#include "ipaddress.h"

#include <cstdint>
#include <list>

#include <net/if.h>

class Vrrp
{
	public:
		Vrrp (const char *interface, int family, const IpAddress &primaryIpAddress, std::uint8_t virtualRouterId, std::uint8_t priority = 100);
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

		bool preemptMode () const;
		void setPreemptMode (bool enabled);

		bool acceptMode () const;
		void setAcceptMode (bool enabled);

		bool addIpAddress (const IpAddress &address);
		bool removeIpAddress (const IpAddress &address);

	private:
		enum State
		{
			Initialize,
			Backup,
			Master
		};

		bool initSocket ();

		bool joinMulticast (int interface);
		bool leaveMulticast (int interface);
		bool modifyMulticast (int interface, bool join);
		void initSocket (const char *interface, int family);

		void startup ();
		void shutdown ();
		bool sendAdvertisement (int priority = -1);

		void onVrrpPacket ();
		void onMasterDownTimer ();
		void onAdvertisementTimer ();

		void setState (State state);

		bool sendARPs ();
		bool sendNeighborAdvertisements ();
		bool joinSolicitedNodeMulticasts ();
		bool setVirtualMac ();
		bool setDefaultMac ();
		bool addMacvlanInterface ();
		bool removeMacvlanInterface ();

		bool addIpAddresses ();
		bool removeIpAddresses ();

		std::uint16_t checksum (const void *packet, unsigned int size, const IpAddress &srcIpAddress, const IpAddress &dstIpAddress) const;

		static void socketCallback (int, void *);
		static void masterDownTimerCallback (Timer *, void *);
		static void advertisementTimerCallback (Timer *, void *);

	private:
		std::uint8_t m_virtualRouterId;
		std::uint8_t m_priority;
		IpAddress m_primaryIpAddress;
		IpAddress m_multicastAddress;
		unsigned int m_advertisementInterval;
		unsigned int m_masterAdvertisementInterval;
		bool m_preemptMode;
		bool m_acceptMode;

		Timer m_masterDownTimer;
		Timer m_advertisementTimer;

		State m_state;
		int m_family;
		int m_socket;
		int m_arpSocket;
		int m_interface;
		int m_outputInterface;

		std::uint8_t m_buffer[1500];
		std::uint8_t m_mac[6];

		typedef std::list<IpAddress> IpAddressList;
		std::list<IpAddress> m_addrs;
};

#endif // INCLUDE_VRRP_H
