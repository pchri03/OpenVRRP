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
			Initialize = 1,
			Backup = 2,
			Master = 3
		};

		enum NewMasterReason
		{
			NotMaster = 1,
			Priority = 2,
			Preempted = 3,
			MasterNotResponding = 4
		};

		enum ProtocolErrorReason
		{
			NoError = 0,
			IpTtlError = 1,
			VersionError = 2,
			ChecksumError = 3,
			VrIdError = 4
		};

		VrrpService (int interface, int family, std::uint_fast8_t virtualRouterId);
		~VrrpService ();

		inline int interface () const
		{
			return m_interface;
		}

		inline int family () const
		{
			return m_family;
		}

		inline std::uint_fast8_t virtualRouterId () const
		{
			return m_virtualRouterId;
		}

		inline IpAddress primaryIpAddress () const
		{
			return m_primaryIpAddress;
		}

		inline IpAddress masterIpAddress () const
		{
			return m_masterIpAddress;
		}

		inline bool setPrimaryIpAddress (const IpAddress &address)
		{
			if (m_state == Initialize && address.family() == m_family)
			{
				m_primaryIpAddress = address;
				return true;
			}
			else
				return false;
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
		const IpAddressList &addresses () const
		{
			return m_addresses;
		}

		inline State state () const
		{
			return m_state;
		}

		void enable ();
		void disable ();

		inline int error () const
		{
			return m_error;
		}

		inline std::uint_fast32_t statsMasterTransitions () const
		{
			return m_statsMasterTransitions;
		}

		inline NewMasterReason statsNewMasterReason () const
		{
			return m_statsNewMasterReason;
		}

		inline std::uint_fast64_t statsRcvdAdvertisements () const
		{
			return m_statsRcvdAdvertisements;
		}

		inline std::uint_fast64_t statsAdvIntervalErrors () const
		{
			return m_statsAdvIntervalErrors;
		}

		inline std::uint_fast64_t statsIpTtlErrors () const
		{
			return m_statsIpTtlErrors;
		}

		inline ProtocolErrorReason statsProtocolErrReason () const
		{
			return m_statsProtocolErrReason;
		}

		inline std::uint_fast64_t statsRcvdPriZeroPackets () const
		{
			return m_statsRcvdPriZeroPackets;
		}

		inline std::uint_fast64_t statsSentPriZeroPackets() const
		{
			return m_statsSentPriZeroPackets;
		}

		inline std::uint_fast64_t statsRcvdInvalidTypePackets () const
		{
			return m_statsRcvdInvalidTypePackets;
		}

		inline std::uint_fast64_t statsAddressListErrors () const
		{
			return m_statsAddressListErrors;
		}

		inline std::uint_fast64_t statsPacketLengthErrors () const
		{
			return m_statsPacketLengthErrors;
		}

		inline bool enabled () const
		{
			return m_enabled;
		}

	private:
		virtual void onIncomingVrrpPacket (
				unsigned int interface,
				const IpAddress &address,
				std::uint_fast8_t virtualRouterId,
				std::uint_fast8_t priority,
				std::uint_fast16_t maxAdvertisementInterval,
				const IpAddressList &addresses);

		virtual void onIncomingVrrpError (unsigned int interface, std::uint_fast8_t virtualRouterId, VrrpEventListener::Error error);

		void startup ();
		void shutdown ();

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

		void setProtocolErrorReason (ProtocolErrorReason reason);

		static void timerCallback (Timer *timer, void *userData);

	private:
		std::uint_fast8_t m_virtualRouterId;
		std::uint_fast8_t m_priority;
		IpAddress m_primaryIpAddress;
		IpAddress m_masterIpAddress;
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
		int m_error;

		std::uint_fast32_t m_statsMasterTransitions;
		NewMasterReason m_statsNewMasterReason;
		std::uint_fast64_t m_statsRcvdAdvertisements;
		std::uint_fast64_t m_statsAdvIntervalErrors;
		std::uint_fast64_t m_statsIpTtlErrors;
		ProtocolErrorReason m_statsProtocolErrReason;
		std::uint_fast64_t m_statsRcvdPriZeroPackets;
		std::uint_fast64_t m_statsSentPriZeroPackets;
		std::uint_fast64_t m_statsRcvdInvalidTypePackets;
		std::uint_fast64_t m_statsAddressListErrors;
		std::uint_fast64_t m_statsPacketLengthErrors;

		NewMasterReason m_pendingNewMasterReason;

		bool m_autoSync;
		bool m_enabled;
};

#endif // INCLUDE_OPENVRRP_VRRPSERVICE_H
