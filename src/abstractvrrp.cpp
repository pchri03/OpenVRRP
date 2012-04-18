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

#include "abstractvrrp.h"
#include "mainloop.h"

#include <cerrno>

#include <unistd.h>

Vrrp::Vrrp (const char *interface, int family, std::uint8_t virtualRouterId, std::uint8_t priority) :
	m_virtualRouterId(virtualRouterId),
	m_priority(priority),
	m_advertisementInterval(100),
	m_masterAdvertisementInterval(m_advertisementInterval),
	m_preemptMode(true),
	m_acceptMode(false),
	m_masterDownTimer(masterDownTimerCallback, this),
	m_advertisementTimer(advertisementTimerCallback, this),
	m_state(Initialize),
	m_family(family),
	m_socket(-1)
{
	initSocket(interface);
}

bool Vrrp::initSocket (const char *interface)
{
	// Create VRRP socket
	m_socket = socket(m_family, SOCK_RAW, 112);
	if (m_socket == -1)
	{
		syslog(LOG_ERR, "Error creating VRRP socket: %m");
		return false;
	}

	// Bind to interface
	if (setsockopt(m_socket, SOL_SOCKET, SO_BINDTODEVICE, interface, std::strlen(interface)) == -1)
		syslog(LOG_WARNING, "Error binding to interface %s: %m", interface);

	// Set TTL to 255
	int val = 255;
	if (setsockopt(m_socket, IPPROTO_IP, IP_TTL, val, sizeof(val)) == -1)
		syslog(LOG_WARNING, "Error setting TTL: %m");
}

Vrrp::~Vrrp ()
{
	if (m_socket != -1)
	{
		MainLoop::removeMonitor(m_socket);
		while (close(m_socket) == -1 && errno == EINTR);
	}
}

void Vrrp::advertisementTimerCallback (Timer *, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
	{
	}
}

void Vrrp::startup ()
{
	if (m_priority == 255)
	{
		sendAdvertisement();
		onStartupEvent(); // Send ARP or ND Neighbor Advertisement
		m_advertisementTimer.start(advertisementInterval());
		setState(Master);
	}
	else
	{
		m_masterAdvertisementInterval = m_advertisementInterval;
		m_masterDownTimer.start(masterDownInterval());
		setState(Backup);
	}
}

void Vrrp::shutdown ()
{
	if (m_state == Backup)
	{
		m_masterDownTimer.stop();
		setState(Initialize);
	}
	else if (m_state == Master)
	{
		m_advertisementTimer.stop();
		sendAdvertisement(0);
		setState(Initialize);
	}
}

void Vrrp::masterDownTimerCallback (Timer *, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
	{
		vrrp->sendAdvertisement();
		vrrp->onMasterDownEvent(); // Send ARP or multicast join followed by ND Neighbor Advertisement
		vrrp->m_advertisementTimer.start(vrrp->advertisementInterval());
	}
}

void Vrrp::socketCallback (int fd, void *userData)
{
	Vrrp *vrrp = reinterpret_cast<Vrrp *>(userData);
	if (vrrp != 0)
	{
		struct sockaddr addr;
		socklen_t addrlen = sizeof(addr);
		std::ssize_t size;
		while ((size = recvfrom(fd, vrrp->m_buffer, sizeof(vrrp->m_buffer), 0, addr, &addrlen)) == -1 && errno == EINTR);

		if (size > 0)
		{
			if (size < 8)
				return;

			// Verify version and type
			if (vrrp->m_buffer[0] != 0x31)	// VRRPv3 ADVERTISEMENT
				return;

			// Verify virtual router id
			if (vrrp->m_buffer[1] != virtualRouterId())
				return;

			std::uint8_t priority = vrrp->m_buffer[3];
			std::uint16_t maxAdvertisementInterval = ((std::uint16_t)(vrrp->m_buffer[4] & 0x0F) << 8) | vrrp->m_buffer[5];

			// TODO - Checksum

			if (vrrp->m_state == Backup)
			{
				if (priority == 0)
					vrrp->m_masterDownTimer.start(vrrp->skewTime());
				else if (!vrrp->preemptMode() || priority >= vrrp->priority())
				{
					vrrp->m_masterAdvertisementInterval = maxAdvertisementInterval;
					vrrp->m_masterDownTimer.start(vrrp->masterDownInterval());
				}
			}
			else if (vrrp->m_state == Master)
			{
				if (priority == 0)
				{
					vrrp->sendAdvertisement();
					vrrp->m_advertisementTimer.start(vrrp->advertisementInterval());
				}
				else if (priority > vrrp->priority()
			}
		}
	}
}
