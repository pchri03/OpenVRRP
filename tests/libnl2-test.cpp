#include <netlink/netlink.h>
#include <netlink/route/addr.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <iostream>
#include <cstdint>
#include <cstring>

#include "mainloop.h"

static bool testAddIpAddress (nl_sock *sock, int interface)
{
	std::cout << "testAddIpAddress()" << std::endl;

	rtnl_addr *addr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(addr, interface);
	rtnl_addr_set_family(addr, AF_INET);

	nl_addr *ip;
	nl_addr_parse("10.44.1.1/24", AF_INET, &ip);
	rtnl_addr_set_local(addr, ip);

	int ret = rtnl_addr_add(sock, addr, 0);

	if (ret != 0)
		std::cerr << " rtnl_addr_add() failed with error " << ret << std::endl;
	
	rtnl_addr_put(addr);

	return (ret == 0);
}

static bool testRemoveIpAddress (nl_sock *sock, int interface)
{
	std::cout << "testRemoveIpAddress()" << std::endl;

	rtnl_addr *addr = rtnl_addr_alloc();
	rtnl_addr_set_ifindex(addr, interface);
	rtnl_addr_set_family(addr, AF_INET);

	nl_addr *ip;
	nl_addr_parse("10.44.1.1/24", AF_INET, &ip);
	rtnl_addr_set_local(addr, ip);

	int ret = rtnl_addr_delete(sock, addr, 0);

	if (ret != 0)
		std::cerr << " rtnl_addr_delete() failed with error " << ret << std::endl;

	rtnl_addr_put(addr);

	return (ret == 0);
}

static bool testGetIpAddresses (nl_sock *sock, int interface)
{
	std::cout << "testGetIpAddresses()" << std::endl;
	nl_cache *cache;
	rtnl_addr_alloc_cache(sock, &cache);

	bool ret = false;
	for (nl_object *it = nl_cache_get_first(cache); it != 0; it = nl_cache_get_next(it))
	{
		rtnl_addr *addr = reinterpret_cast<rtnl_addr *>(it);
		if (rtnl_addr_get_ifindex(addr) == interface && (rtnl_addr_get_flags(addr) & 0x80)) // IFA_F_PERMANENT
		{
			char buffer[32];
			nl_addr *local = rtnl_addr_get_local(addr);
			std::cout << " Found " << nl_addr2str(local, buffer, sizeof(buffer)) << std::endl;
			ret = true;
			break;
		}
	}

	if (ret == false)
		std::cerr << " testGetIpAddresses() failed" << std::endl;

	nl_cache_free(cache);

	return true;
}

static bool testGetInterfaces (nl_sock *sock)
{
	std::cout << "testGetInterfaces()" << std::endl;
	nl_cache *cache;
	rtnl_link_alloc_cache(sock, &cache);

	for (nl_object *it = nl_cache_get_first(cache); it != 0; it = nl_cache_get_next(it))
	{
		rtnl_link *link = reinterpret_cast<rtnl_link *>(it);
		std::cout << ' ' << rtnl_link_get_ifindex(link) << ": " << rtnl_link_get_name(link) << std::endl;
	}
	nl_cache_free(cache);
	return true;
}

static int testAddInterface (nl_sock *sock)
{
	std::cout << "testAddInterface()" << std::endl;

	static const char name[] = "test";
	static const std::uint8_t macAddress[6] = {0x00, 0x00, 0x5E, 0x00, 0x01, 0x01};

	nl_msg *msg = nlmsg_alloc_simple(RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK);
	ifinfomsg infomsg;

	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;

	// macvlan link info data
	nl_msg *infodata = nlmsg_alloc();
	nla_put_u32(infodata, IFLA_MACVLAN_MODE, MACVLAN_MODE_PRIVATE);

	// Link info
	nl_msg *linkinfo = nlmsg_alloc();
	nla_put_string(linkinfo, IFLA_INFO_KIND, "macvlan");
	nla_put_nested(linkinfo, IFLA_INFO_DATA, infodata);

	int err;

	if ((err = nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO)) != 0)
	{
		std::cerr << " nlmsg_append() failed with error: " << nl_geterror(err) << std::endl;
		return -1;
	}

	nla_put_string(msg, IFLA_IFNAME, name);
	nla_put(msg, IFLA_ADDRESS, 6, macAddress);
	nla_put_u32(msg, IFLA_OPERSTATE, 6); // IF_OPER_UP
	nla_put_u32(msg, IFLA_LINK, 2); // eth0
	nla_put_nested(msg, IFLA_LINKINFO, linkinfo);

	err = nl_send_auto_complete(sock, msg);

	nlmsg_free(infodata);
	nlmsg_free(linkinfo);
	nlmsg_free(msg);

	if (err < 0)
	{
		std::cerr << " nl_send_auto_complete() failed with error: " << nl_geterror(err) << std::endl;
		return -1;
	}

	err = nl_wait_for_ack(sock);

	if (err != 0)
	{
		std::cerr << " nl_wait_for_ack() failed with error: " << nl_geterror(err) << std::endl;
		return -1;
	}

	int interface = if_nametoindex(name);
	std::cout << " Created interface " << interface << std::endl;

	return interface;
}

static bool testRemoveInterface (nl_sock *sock, int interface)
{
	std::cout << "testRemoveInterface()" << std::endl;

	nl_msg *msg = nlmsg_alloc_simple(RTM_DELLINK, NLM_F_REQUEST | NLM_F_ACK);
	ifinfomsg infomsg;

	std::memset(&infomsg, 0, sizeof(infomsg));
	infomsg.ifi_family = AF_UNSPEC;
	infomsg.ifi_type = ARPHRD_ETHER;
	infomsg.ifi_index = interface;

	nlmsg_append(msg, &infomsg, sizeof(infomsg), NLMSG_ALIGNTO);

	int err = nl_send_auto_complete(sock, msg);

	nlmsg_free(msg);

	if (err < 0)
	{
		std::cerr << " nl_send_auto_complete() failed with error: " << nl_geterror(err) << std::endl;
		return false;
	}

	err = nl_wait_for_ack(sock);

	if (err != 0)
	{
		std::cerr << " nl_wait_for_ack() failed with error: " << nl_geterror(err) << std::endl;
		return false;
	}

	return true;
}

static void testEventsCallback (int, void *userData)
{
	nl_sock *sock = reinterpret_cast<nl_sock *>(userData);
	nl_recvmsgs_default(sock);
}

static int testEventsIncomingMessage (nl_msg *msg, void *)
{
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
	std::cout << "testEvents()" << std::endl;

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
		std::cerr << "nl_connect() failed" << std::endl;
		return -1;
	}

	testGetInterfaces(sock);
	int interface = testAddInterface(sock);
	if (interface != -1)
	{
		testAddIpAddress(sock, interface);
		testGetIpAddresses(sock, interface);
		testRemoveIpAddress(sock, interface);
		testRemoveInterface(sock, interface);
	}
	testEvents(sock);

	nl_socket_free(sock);

	return 0;
}
