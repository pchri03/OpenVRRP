#include <netlink/netlink.h>
#include <netlink/route/addr.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <net/if.h>

#include <iostream>

#include "mainloop.h"

static bool testAddIpAddress (nl_sock *sock)
{
	rtnl_addr *addr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(addr, 2);
	rtnl_addr_set_family(addr, AF_INET);

	nl_addr *ip;
	nl_addr_parse("10.44.1.1/24", AF_INET, &ip);
	rtnl_addr_set_local(addr, ip);

	int ret = rtnl_addr_add(sock, addr, 0);

	if (ret == 0)
		std::cout << "rtnl_addr_add succeeded" << std::endl;
	else
		std::cerr << "rtnl_addr_add failed with error " << ret << std::endl;
	
	rtnl_addr_put(addr);

	return (ret == 0);
}

static bool testRemoveIpAddress (nl_sock *sock)
{
	rtnl_addr *addr = rtnl_addr_alloc();
	rtnl_addr_set_ifindex(addr, 2);
	rtnl_addr_set_family(addr, AF_INET);

	nl_addr *ip;
	nl_addr_parse("10.44.1.1/24", AF_INET, &ip);
	rtnl_addr_set_local(addr, ip);

	int ret = rtnl_addr_delete(sock, addr, 0);

	if (ret == 0)
		std::cout << "rtnl_addr_delete succeeded" << std::endl;
	else
		std::cerr << "rtnl_addr_delete failed with error " << ret << std::endl;

	rtnl_addr_put(addr);

	return (ret == 0);
}

static bool testGetIpAddresses (nl_sock *sock)
{
	nl_cache *cache;
	rtnl_addr_alloc_cache(sock, &cache);

	bool ret = false;
	for (nl_object *it = nl_cache_get_first(cache); it != 0; it = nl_cache_get_next(it))
	{
		rtnl_addr *addr = reinterpret_cast<rtnl_addr *>(it);
		if (rtnl_addr_get_ifindex(addr) == 2 && (rtnl_addr_get_flags(addr) & 0x80)) // IFA_F_PERMANENT
		{
			char buffer[32];
			nl_addr *local = rtnl_addr_get_local(addr);
			std::cout << "testGetIpAddresses returned " << nl_addr2str(local, buffer, sizeof(buffer)) << std::endl;
			ret = true;
			break;
		}
	}

	if (ret == false)
		std::cerr << "testGetIpAddresses failed" << std::endl;

	nl_cache_free(cache);

	return true;
}

static void testEventsCallback (int, void *userData)
{
	nl_sock *sock = reinterpret_cast<nl_sock *>(userData);
	nl_recvmsgs_default(sock);
}

static int testEventsIncomingMessage (nl_msg *msg, void *arg)
{
	std::cerr << "testEventsIncomingMessage()" << std::endl;

	nlmsghdr *hdr = nlmsg_hdr(msg);
	if (hdr->nlmsg_type == RTM_NEWLINK)
	{
		const ifinfomsg *msg = reinterpret_cast<const ifinfomsg *>(nlmsg_data(hdr));
		if (msg->ifi_change & IFF_UP)
		{
			std::cout << " Interface " << msg->ifi_index << " is now " << ((msg->ifi_flags & IFF_UP) == IFF_UP ? "up" : "down") << std::endl;
		}
	}

	return 0;
}

static bool testEvents (nl_sock *sock)
{
	if (nl_socket_add_membership(sock, RTMGRP_LINK) != 0)
	{
		std::cerr << "nl_socket_add_membership failed" << std::endl;
		return false;
	}

	nl_socket_set_nonblocking(sock);

	nl_socket_modify_cb(sock, NL_CB_MSG_IN, NL_CB_CUSTOM, testEventsIncomingMessage, sock);

	MainLoop::addMonitor(nl_socket_get_fd(sock), testEventsCallback, sock);
	MainLoop::run();

	nl_socket_drop_membership(sock, RTMGRP_LINK);

	return true;
}

int main ()
{
	nl_sock *sock = nl_socket_alloc();

	if (nl_connect(sock, NETLINK_ROUTE) != 0)
	{
		std::cerr << "nl_connect failed" << std::endl;
		return -1;
	}

	testAddIpAddress(sock);
	testGetIpAddresses(sock);
	testRemoveIpAddress(sock);
	testEvents(sock);

	nl_socket_free(sock);

	return 0;
}
