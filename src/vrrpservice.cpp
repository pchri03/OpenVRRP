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

#include "netlink.h"
#include "vrrpservice.h"
#include "vrrpsocket.h"

#include <algorithm>

#include <syslog.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>

VrrpService::VrrpService (int interface, int family, std::uint_fast8_t virtualRouterId) :
	m_virtualRouterId(virtualRouterId),
	m_priority(100),
	m_primaryIpAddress(Netlink::getPrimaryIpAddress(interface, family)),
	m_advertisementInterval(100),
	m_masterAdvertisementInterval(m_advertisementInterval),
	m_preemptMode(true),
	m_acceptMode(false),
	m_masterDownTimer(timerCallback, this),
	m_advertisementTimer(timerCallback, this),
	m_state(Initialize),
	m_family(family),
	m_interface(interface),
	m_outputInterface(interface),
	m_socket(VrrpSocket::instance(m_family)),
	m_error(0),

	m_statsMasterTransitions(0),
	m_statsNewMasterReason(NotMaster),
	m_statsRcvdAdvertisements(0),
	m_statsAdvIntervalErrors(0),
	m_statsIpTtlErrors(0),
	m_statsProtocolErrReason(NoError),
	m_statsRcvdPriZeroPackets(0),
	m_statsSentPriZeroPackets(0),
	m_statsRcvdInvalidTypePackets(0),
	m_statsAddressListErrors(0),
	m_statsPacketLengthErrors(0),

	m_pendingNewMasterReason(MasterNotResponding),

	m_autoSync(false)
{
	if (m_family == AF_INET)
		m_name = "VRRP IPv4 Service";
	else // if (m_family == AF_INET6)
		m_name = "VRRP IPv6 Service";

	m_mac[0] = 0x00;
	m_mac[1] = 0x00;
	m_mac[2] = 0x5E;
	m_mac[3] = 0x00;
	m_mac[4] = (family == AF_INET ? 1 : 2);
	m_mac[5] = virtualRouterId;

	if (m_socket != 0)
	{
		char name[sizeof("vrrp.xxxxxxxxxxx.xxx.x")];
		std::sprintf(name, "vrrp.%i.%hhu.%u", interface, virtualRouterId, (family == AF_INET ? 1 : 2));
		m_outputInterface = Netlink::addMacvlanInterface(m_interface, m_mac, name);
		if (m_outputInterface < 0)
			m_outputInterface = m_interface;

		m_socket->addEventListener(m_interface, m_virtualRouterId, this);
	}
	else
		m_error = 1;
}

VrrpService::~VrrpService ()
{
	shutdown();

	if (m_outputInterface != m_interface)
		Netlink::removeInterface(m_outputInterface);

	if (m_socket != 0)
		m_socket->removeEventListener(m_interface, m_virtualRouterId);
}

void VrrpService::timerCallback (Timer *timer, void *userData)
{
	VrrpService *self = reinterpret_cast<VrrpService *>(userData);
	if (timer == &self->m_masterDownTimer)
		self->onMasterDownTimer();
	else if (timer == &self->m_advertisementTimer)
		self->onAdvertisementTimer();
}

void VrrpService::startup ()
{
	if (m_priority == 255)
	{
		// We are starting up and we are the owner of the virtual IP addresses, so transition to master immediately
		setState(Master);
		if (m_family == AF_INET)
			sendARPs();
		else // if (m_family == AF_INET6)
			sendNeighborAdvertisements();

		m_advertisementTimer.start(m_advertisementInterval * 10);
		
		// Update statistics
		++m_statsMasterTransitions;
		m_statsNewMasterReason = Preempted;
	}
	else
	{
		// We are starting up, but we are not the owner. Transition to backup and wait for an advertisement from a master
		m_masterAdvertisementInterval = m_advertisementInterval;
		m_masterDownTimer.start(masterDownInterval() * 10);
		setState(Backup);
		m_statsNewMasterReason = NotMaster;
	}
}

void VrrpService::shutdown ()
{
	if (state() == Backup)
	{
		// We are backup, so just stop our timer and switch to initialize state
		m_masterDownTimer.stop();
		setState(Initialize);
	}
	else if (state() == Master)
	{
		// We are master, so inform everybody that we're leaving
		m_advertisementTimer.stop();
		sendAdvertisement(0);
		setState(Initialize);
		
		++m_statsSentPriZeroPackets;
	}

}

void VrrpService::onMasterDownTimer ()
{
	if (m_state == Backup)
	{
		// We are backup and the master down timer triggered, so we should transition to master
		setState(Master);
		if (m_family == AF_INET)
			sendARPs();
		else if (m_family == AF_INET6)
		{
			joinSolicitedNodeMulticast();
			sendNeighborAdvertisements();
		}
		m_advertisementTimer.start(m_advertisementInterval * 10);

		// Update statistics
		++m_statsMasterTransitions;
		m_statsNewMasterReason = m_pendingNewMasterReason;
	}
}

void VrrpService::onAdvertisementTimer ()
{
	if (m_state == Master)
	{
		// We are master and the advertisement timer fired, so send an advertisement
		sendAdvertisement(m_priority);
		m_advertisementTimer.start(m_advertisementInterval * 10);
	}
}

void VrrpService::onIncomingVrrpPacket (
		unsigned int,
		const IpAddress &address,
		std::uint_fast8_t,
		std::uint_fast8_t priority,
		std::uint_fast16_t maxAdvertisementInterval,
		const IpAddressList &addresses)
{
	++m_statsRcvdAdvertisements;
	m_statsProtocolErrReason = NoError;

	if (m_state == Backup)
	{
		if (priority == 0)
		{
			// The master decided to stop gracefully, wait skew time before transitioning to master
			m_masterDownTimer.start(skewTime() * 10);

			++m_statsRcvdPriZeroPackets;
			m_pendingNewMasterReason = Priority;
		}
		else if (!m_preemptMode || priority >= this->priority())
		{
			// The right master is running, wait for the next announcement
			m_masterAdvertisementInterval = maxAdvertisementInterval;
			m_masterDownTimer.start(masterDownInterval() * 10);

			// Check address list
			std::vector<IpAddress> incomingList(addresses.size());
			std::copy(addresses.begin(), addresses.end(), incomingList.begin());
			std::sort(incomingList.begin(), incomingList.end());
			
			std::vector<IpAddress> serviceList(m_addresses.size());
			std::copy(m_addresses.begin(), m_addresses.end(), serviceList.begin());
			std::sort(serviceList.begin(), serviceList.end());

			std::vector<IpAddress> difference(std::max(incomingList.size(), serviceList.size()));
			std::vector<IpAddress>::iterator differenceEnd = std::set_difference(incomingList.begin(), incomingList.end(), serviceList.begin(), serviceList.end(), difference.begin());

			std::vector<IpAddress>::size_type differenceCount = differenceEnd - difference.begin();
			if (differenceCount > 0)
			{
				// There are differences between incoming address list and our list
				syslog(LOG_WARNING, "%s (Router %u, Interface %u): Address list mismatch", m_name, (unsigned int)m_virtualRouterId, (unsigned int)m_interface);
				++m_statsAddressListErrors;

				// If allowed, update our address list to reflect that of the master
				if (m_autoSync)
				{
					std::vector<IpAddress> list(differenceCount);
					std::vector<IpAddress>::iterator listEnd;
					
					listEnd = std::set_intersection(difference.begin(), differenceEnd, incomingList.begin(), incomingList.end(), list.begin());
					for (std::vector<IpAddress>::iterator addr = list.begin(); addr != listEnd; ++addr)
					{
						addIpAddress(*addr);
					}

					listEnd = std::set_intersection(difference.begin(), differenceEnd, serviceList.begin(), serviceList.end(), list.begin());
					for (std::vector<IpAddress>::iterator addr = list.begin(); addr != listEnd; ++addr)
					{
						removeIpAddress(*addr);
					}

					syslog(LOG_NOTICE, "%s (Router %u, Interface %u): Synchronized address list", m_name, (unsigned int)m_virtualRouterId, (unsigned int)m_interface);
				}
			}

			m_pendingNewMasterReason = MasterNotResponding;
		}
		else if (m_preemptMode)
			m_pendingNewMasterReason = Preempted;
		else
			m_pendingNewMasterReason = Priority;
	}
	else if (m_state == Master)
	{
		if (priority == 0)
		{
			// The conflicing master is stopping gracefully, so just remind everybody that we are the master
			sendAdvertisement(m_priority);
			m_advertisementTimer.start(advertisementInterval() * 10);

			++m_statsRcvdPriZeroPackets;
			m_pendingNewMasterReason = Priority;
		}
		else if (priority > m_priority || priority == m_priority && address > m_primaryIpAddress)
		{
			// The conflicing master has higher priority than us, so we transition to backup
			m_advertisementTimer.stop();
			m_masterAdvertisementInterval = maxAdvertisementInterval;
			m_masterDownTimer.start(masterDownInterval() * 10);
			setState(Backup);
			setDefaultMac();

			m_pendingNewMasterReason = Priority;
		}
		else
		{
			// The conflicing master has a lower priority than us. We expect it to transition to backup
		}
	}
}

bool VrrpService::sendAdvertisement (std::uint_least8_t priority)
{
	return m_socket->sendPacket(
			m_outputInterface,
			m_primaryIpAddress,
			m_virtualRouterId,
			priority,
			m_advertisementInterval,
			m_addresses);
}

void VrrpService::setState (State state)
{
	if (m_state != state)
	{
		static const char *states[] = {"Initialize", "Backup", "Master"};
		m_state = state;
		if (m_state == Master)
		{
			static const char *reasons[] = {"NotMaster", "Priority", "Preempted", "MasterNotResponding"};
			syslog(LOG_INFO, "%s (Router %u, Interface %u): Changed state to %s (Reason: %s)", m_name, (unsigned int)m_virtualRouterId, (unsigned int)m_interface, states[m_state - 1], reasons[m_pendingNewMasterReason - 1]);
		}
		else
			syslog(LOG_INFO, "%s (Router %u, Interface %u): Changed state to %s", m_name, (unsigned int)m_virtualRouterId, (unsigned int)m_interface, states[m_state - 1]);
		if (m_state == Master)
		{
			if (m_outputInterface != m_interface)
				Netlink::toggleInterface(m_outputInterface, true);
			setVirtualMac();
			addIpAddresses();
			sendAdvertisement(m_priority);
		}
		else
		{
			removeIpAddresses();
			setDefaultMac();
		}
	}
}

void VrrpService::sendARPs ()
{
	// TODO
}

void VrrpService::sendNeighborAdvertisements ()
{
	// TODO
}

void VrrpService::joinSolicitedNodeMulticast ()
{
	// TODO
}

bool VrrpService::setVirtualMac ()
{
	if (m_outputInterface == m_interface)
		return Netlink::setMac(m_outputInterface, m_mac);
	else
		return Netlink::toggleInterface(m_outputInterface, true);
}

bool VrrpService::setDefaultMac ()
{
	if (m_outputInterface == m_interface)
	{
		/*
		struct
		{
			std::uint32_t cmd;
			std::uint32_t size;
			std::uint8_t mac[6];
		} packet;
		packet.cmd = ETHTOOL_GPERMADDR;
		packet.size = 6;
		
		struct ifreq req;
		if_indextoname(m_interface, req.ifr_ifrn.ifrn_name);
		req.ifr_ifru.ifru_data = reinterpret_cast<__caddr_t>(&packet);

		if (ioctl(m_socket, SIOCETHTOOL, &req) == -1)
		{
			syslog(LOG_ERR, "Error getting permanent hardware address: %m");
			return false;
		}

		return Netlink::setMac(m_outputInterface, packet.mac);
		*/
		return true;
	}
	else
		return Netlink::toggleInterface(m_outputInterface, false);
}

bool VrrpService::addIpAddresses ()
{
	bool ret = true;
	for (IpAddressList::const_iterator addr = m_addresses.begin(); addr != m_addresses.end(); ++addr)
		ret &= (Netlink::addIpAddress(m_outputInterface, *addr));
	return ret;
}

bool VrrpService::removeIpAddresses ()
{
	bool ret = true;
	for (IpAddressList::const_iterator addr = m_addresses.begin(); addr != m_addresses.end(); ++addr)
		ret &= (Netlink::removeIpAddress(m_outputInterface, *addr));
	return ret;
}

bool VrrpService::addIpAddress (const IpAddress &address)
{
	if (address.family() != m_family)
		return false;
	m_addresses.push_back(address);
	return true;
}

bool VrrpService::removeIpAddress (const IpAddress &address)
{
	// TODO
	return false;
}

void VrrpService::setProtocolErrorReason (ProtocolErrorReason reason)
{
	m_statsProtocolErrReason = reason;
	// TODO Send SNMP notificaiton
}

void VrrpService::onIncomingVrrpError (unsigned int interface, std::uint_fast8_t virtualRouterId, VrrpEventListener::Error error)
{
	switch (error)
	{
		case VrrpEventListener::ChecksumError:
			setProtocolErrorReason(ChecksumError);
			break;

		case VrrpEventListener::VersionError:
			setProtocolErrorReason(VersionError);
			break;

		case VrrpEventListener::VrIdError:
			setProtocolErrorReason(VrIdError);
			break;
			
		case VrrpEventListener::AdvIntervalError:
			++m_statsAdvIntervalErrors;
			break;

		case VrrpEventListener::IpTtlError:
			setProtocolErrorReason(IpTtlError);
			break;

		case VrrpEventListener::InvalidTypeError:
			++m_statsRcvdInvalidTypePackets;
			break;

		case VrrpEventListener::PacketLengthError:
			++m_statsPacketLengthErrors;
			break;
	}
}
