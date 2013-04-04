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
#include "ipsubnet.h"
#include "timer.h"
#include "vrrpeventlistener.h"

#include <cstdint>

class VrrpSocket;

/**
  * VRRP instance
  *
  * The VrrpService class is where all the VRRPv3 magic lies.
  */
class VrrpService : private VrrpEventListener
{
	public:
		enum State
		{
			Disabled = 0, // VRRPV3-MIB::vrrpv3OperationsStatus = initialize(1), VRRPV3-MIB::vrrpv3OperationsRowStatus = notInService(2)
			LinkDown, // VRRPV3-MIB::vrrpv3OperationsStatus = initialize(1), VRRPV3-MIB::vrrpv3OperationsRowStatus = notReady(3)
			Backup, // VRRPV3-MIB::vrrpv3OperationsStatus = backup(2), VRRPV3-MIB::vrrpv3OperationsRowStatus = active(1)
			Master // VRRRPV3-MIB::vrrpv3OperationsStatus = master(3), VRRPV3-MIB::vrrpv3OperationsRowStatus = active(1)
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

		/**
		  * Construct new VrrpService
		  *
		  * Be sure to call error() to see if the construction completed successfully
		  * @param interface Index of network interface to run VRRP service on
		  * @param family Address family. Can be either AF_INET or AF_INET6
		  * @param virtualRouterId Virtual router id
		  */
		VrrpService (int interface, int family, std::uint_fast8_t virtualRouterId);
		~VrrpService ();

		/**
		  * Get the internal error code
		  *
		  * The only way error() can return non-zero, is if the constructor failed
		  * @return 0 if service is good and 1 otherwise
		  */
		int error () const;

		/**
		  * Get interface the service runs on
		  * @return Interface index
		  */
		int interface () const;

		/**
		  * Get address family of service
		  * @return Address family (Either AF_INET or AF_INET6)
		  */
		int family () const;

		/**
		  * Get router id of service
		  * @return Virtual router id
		  */
		std::uint_fast8_t virtualRouterId () const;

		/**
		  * Get the MAC address of the service
		  * @return 48-bit MAC address
		  */
		const std::uint8_t *mac () const;

		/**
 		  * Set the primary IP address of this VRRP router
		  *
		  * The primary IP address is the address used when sending out VRRP
		  * packets. When this router becomes master, all backup routers
		  * will register this IP as the master IP address.
		  *
		  * If no primary IP address is explicitly set, the first available
		  * IP address on the network interface is used. Calling
		  * unsetPrimaryIpAddress() will revert to that behavior.
		  * @param address Primary IP address to use. It must be of the same address family as the service
		  * @return Success state. If operation could not be completed, false is returned.
		  * @see unsetPrimaryIpAddress()
		  * @see primaryIpAddress()
		  * @see hasAutoPrimaryIpAddresss()
		  */
		bool setPrimaryIpAddress (const IpAddress &address);

		/**
		  * Unset the forced primary IP address
		  * 
		  * Call this function to revert the behavior of setPrimaryIpAddress(const IpAddress&).
		  * The new primary IP address will be the first IP address on the network interface
		  * @return Success State. If operation could not be comleted, false is returned.
		  */
		void unsetPrimaryIpAddress ();

		/**
		  * Get primary IP address of VRRP instance
		  * @return Current primary IP address of the VRRP instance
		  */
		IpAddress primaryIpAddress () const;

		/**
		  * Check how the primary IP address was determined
		  *
		  * This method basically checks whether the primary IP address was determined automatically
		  * or explicitly set by setPrimaryIpAddress
		  * @return If the primary IP address was automatically determined, this method returns true.
		  */
		bool hasAutoPrimaryIpAddress () const;

		/**
		  * Get the IP address of the master VRRP router
		  *
		  * If no master could be termined yet (within the first 3-4 seconds),
		  * an invalid IP address is returned (Address family AF_UNSPEC)
		  * @return Ip addresss of the VRRP master
		  */
		IpAddress masterIpAddress () const;

		/**
		  * Set priority of VRRP router
		  *
		  * The priority determines who becomes master.
		  * The priority must be between 0 and 255
		  * @param priority New priority
		  */
		bool setPriority (std::uint_fast8_t priority);

		/**
		  * Get priority of VRRP router
		  * @return Priority
		  */
		std::uint_fast8_t priority () const;

		/**
		  * Set advertisement interval
		  *
		  * The advertisement interval indicates how often the router will send out VRRP announcements
		  * when it becomes master.
		  *
		  * Advertisement intervals must be between 1 (10ms) and 4095 (4.095 sec)
		  * @param advertisementInterval New advertisement interval measured in units of 10 msecs
		  */
		bool setAdvertisementInterval (unsigned int advertisementInterval);

		/**
		  * Get current advertisement interval
		  *
		  * Bare in mind that this is not the same as the advertisement interval of
		  * the master router. See masterAdvertisementInterval() for that value.
		  * @return The advertisement interval of this VRRP router measured in units of 10ms
		  */
		unsigned int advertisementInterval () const;

		/**
		  * Get the advertisement interval of the master VRRP router
		  * @return The advertisement of the master VRRP router measured in units of 10ms
		  */
		inline unsigned int masterAdvertisementInterval () const;

		/**
		  * Get skew time
		  *
		  * The skew time is the time the VRRP router will wait after the master is considered
		  * down (3 * masterAdvertisementInterval()), before taking over the master role.
		  *
		  * The skew time is not directly configurable, but is calculated from the priority
		  * and master advertisement interval:
		  *
		  * ((256 - priority) * masterAdvertisementInterval) / 256
		  *
		  * @return Skew time measured in units of 10 ms
		  */
		unsigned int skewTime () const;

		/**
		  * Get time interval for backup to declare master down
		  *
		  * The master down interval is calculated as:
		  * 3 * masterAdvertisementInterval + skewTime
		  * @return Master down interval measured in units of 10ms
		  */
		unsigned int masterDownInterval () const;
		
		/**
		  * Set preemption mode
		  *
		  * When preemption is enabled, the router will immediately take over the master role
		  * if the router was a higher priority than the current master. If preemption is disabled
		  * the router will only take over the master role if it hasn't heard from the master for
		  * masterDownInterval().
		  *
		  * Routers with a priority of 255 will always preempt any other master router
		  * @param enabled Set to true to enable preemption mode
		  */
		void setPreemptMode (bool enabled);

		/**
		  * Get current preemption mode
		  * @return bool true if preemption is enabled
		  */
		bool preemptMode () const;

		/**
		  * Set accept mode
		  *
		  * Since VRRP was only designed to handle redundancy of first-hop routers, true VRRPv2 routers
		  * actually doesn't have the virtual IP addresses associated with its interfaces - only the router
		  * with priority 255 will do this. Instead, VRRP really just fails over who should respond to ARP requests.
		  * This is the default behavior and is indicated by disabling accept mode
		  *
		  * If the virtual IP addresses should be associated to the non-owner routers when they become masters,
		  * enable accept mode. This makes sense when using VRRP as IP-failover on two MySQL servers for instance.
		  * @param enabled New value of accept mode.
		  */
		void setAcceptMode (bool enabled);

		/**
		  * Get accept mode
		  *
		  * Get the current state of accept mode
		  * @return true if accept mode is enabled
		  */
		bool acceptMode () const;

		/**
		  * Add IP address to router
		  *
		  * The IP address is expressed with a CIDR subnet, which is necessary for proper behavior if accept mode is true
		  *
		  * The address must be of the same address family as the router.
		  * @param subnet Subnet to add
		  * @return true if the address was added successfully.
		  */
		bool addIpAddress (const IpSubnet &subnet);

		/**
		  * Remove IP address from router
		  *
		  * The IP address is expressed with a CIDR subnet, which is necessary for proper behavior if accept mode is true.
		  * The subnet spec must be 100% identical to that of addIpAddress
		  * @param subnet Subnet to remove
		  * @return trie if the address as removed successfully
		  */
		bool removeIpAddress (const IpSubnet &subnet);

		/**
		  * Get the list of subnets associated with the VRRP router
		  * @return All subnets added with addIpAddress(const IpAddress&)
		  */
		const IpSubnetSet &subnets () const;

		/**
		  * Get the list of addresses associated with the VRRP router
		  *
		  * This list is basically the same as subnets() but without subnet information
		  * @return All addresses added with addIpAddress(const IpAddress&)
		  */
		const IpAddressSet &addresses () const;

		/**
		  * Get current state of the VRRP router
		  * @return State of router
		  */
		State state () const;

		/**
		  * Enable router
		  */
		void enable ();

		/**
		  * Disable router
		  */
		void disable ();

		/**
		  * Check if service is enabled
		  * @return true if VRRP instance is enabled
		  */
		bool enabled () const;

		/**
		  * Set command to run when the router transition to master
		  *
		  * The command is run with /bin/sh
		  * @param command Command to run
		  */
		void setMasterCommand (const std::string &command);

		/**
		  * Get command run when the router transition to master
		  * @return Command to run
		  */
		std::string masterCommand () const;

		/**
		  * Set command to run when the router transition to backup
		  *
		  * The command is run with /bin/sh
		  * @param command Command to run
		  */
		void setBackupCommand (const std::string &command);

		/**
		  * Get command being wun when the router transition to backup
		  * @return Command to run
		  */
		std::string backupCommand () const;

		/**
		  * Get number of master transitions
		  *
		  * Each time a new router becomes master, this value is incremented
		  * @return Number of master transitions
		  */
		std::uint_fast32_t statsMasterTransitions () const;

		/**
		  * Get the reason for the last master transition
		  * @return Reason for the last master transition
		  */
		NewMasterReason statsNewMasterReason () const;

		/**
		  * Get number of received VRRP advertisements
		  * @return Number of received VRRP advertisements
		  */
		std::uint_fast64_t statsRcvdAdvertisements () const;

		/**
		  * Get number of advertisement interval mismatches
		  *
		  * Although technically not an error in VRRPv3, this counter counts the occurrences
		  * of VRRP advertisements with different advertisement intervals than ours
		  * @return Number of advertisement interval mismatches
		  */
		std::uint_fast64_t statsAdvIntervalErrors () const;

		/**
		  * Get number of TTL errors
		  *
		  * This counter counts the number of VRRP advertisements received where the TTL was not
		  * 255
		  * @return Number of TTL errors
		  */
		std::uint_fast64_t statsIpTtlErrors () const;

		/**
		  * Get the reason for the last protocol error
		  * @return Last protocol error
		  */
		ProtocolErrorReason statsProtocolErrReason () const;

		/**
		  * Get number of zero priority packets received
		  * @return Number of zero priority packets received
		  */
		std::uint_fast64_t statsRcvdPriZeroPackets () const;

		/**
		  * Get the number of zero priority packets sent
		  * @return Number of zero priority packets sent
		  */
		std::uint_fast64_t statsSentPriZeroPackets() const;

		/**
		  * Get the number of invalid VRRP packet types received
		  * @return Number of invalid VRRP packet types received
		  */
		std::uint_fast64_t statsRcvdInvalidTypePackets () const;

		/**
		  * Get the number of address list errors occured
		  *
		  * When the address list sent by the VRRP master is not identical
		  * to the backup routers address list, this counter is incremented
		  * @return Number of address list errors
		  */
		std::uint_fast64_t statsAddressListErrors () const;

		/**
		  * Get the number of packet length errors occured
		  * @return Number of packet length errors occured
		  */
		std::uint_fast64_t statsPacketLengthErrors () const;

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
		void shutdown (State state);

		void onMasterDownTimer ();
		void onAdvertisementTimer ();

		bool sendAdvertisement (std::uint_least8_t priority);
		void sendARPs();
		bool setVirtualMac();
		bool setDefaultMac();
		void setState (State state);
		bool addIpAddresses ();
		bool removeIpAddresses ();

		void setProtocolErrorReason (ProtocolErrorReason reason);

		void executeScript (const std::string &command);

		static void timerCallback (Timer *timer, void *userData);
		static void interfaceCallback (int interface, bool isUp, void *userData);

	private:
		std::uint_fast8_t m_virtualRouterId;
		std::uint_fast8_t m_priority;
		IpAddress m_primaryIpAddress;
		bool m_autoPrimaryIpAddress;
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

		IpSubnetSet m_subnets;
		IpAddressSet m_addresses;

		VrrpSocket *m_socket;

		std::string m_backupCommand;
		std::string m_masterCommand;		

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
		bool m_enabled;
};

#endif // INCLUDE_OPENVRRP_VRRPSERVICE_H
